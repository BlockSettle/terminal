#include "AuthAddressManager.h"
#include <spdlog/spdlog.h>
#include <memory>
#include <QtConcurrent/QtConcurrentRun>
#include "AddressVerificator.h"
#include "ApplicationSettings.h"
#include "CelerClient.h"
#include "CheckRecipSigner.h"
#include "ClientClasses.h"
#include "ConnectionManager.h"
#include "FastLock.h"
#include "HDWallet.h"
#include "OTPManager.h"
#include "PyBlockDataManager.h"
#include "RequestReplyCommand.h"
#include "SafeBtcWallet.h"
#include "SignContainer.h"
#include "WalletsManager.h"
#include "ZmqSecuredDataConnection.h"


using namespace Blocksettle::Communication;


AuthAddressManager::AuthAddressManager(const std::shared_ptr<spdlog::logger> &logger)
   : logger_(logger), QObject(nullptr)
{}

void AuthAddressManager::init(const std::shared_ptr<ApplicationSettings>& appSettings
   , const std::shared_ptr<WalletsManager>& walletsManager
   , const std::shared_ptr<OTPManager>& otpManager)
{
   settings_ = appSettings;
   walletsManager_ = walletsManager;
   otpManager_ = otpManager;

   connect(walletsManager_.get(), &WalletsManager::blockchainEvent, this, &AuthAddressManager::VerifyWalletAddresses);
   connect(walletsManager_.get(), &WalletsManager::authWalletChanged, this, &AuthAddressManager::onAuthWalletChanged);
   connect(otpManager_.get(), &OTPManager::OTPImported, this, &AuthAddressManager::VerifyWalletAddresses);

   SetAuthWallet();
}

void AuthAddressManager::SetSigningContainer(const std::shared_ptr<SignContainer> &container)
{
   signingContainer_ = container;
   connect(signingContainer_.get(), &SignContainer::TXSigned, this, &AuthAddressManager::onTXSigned);
   connect(signingContainer_.get(), &SignContainer::Error, this, &AuthAddressManager::onWalletFailed);
   connect(signingContainer_.get(), &SignContainer::HDLeafCreated, this, &AuthAddressManager::onWalletCreated);
}

void AuthAddressManager::ConnectToPublicBridge(const std::shared_ptr<ConnectionManager> &connMgr
   , const std::shared_ptr<CelerClient>& celerClient)
{
   connectionManager_ = connMgr;
   celerClient_ = celerClient;

   QtConcurrent::run(this, &AuthAddressManager::SendGetBSAddressListRequest);
}

void AuthAddressManager::SetAuthWallet()
{
   if (authWallet_) {
      disconnect(authWallet_.get(), SIGNAL(addressesAdded()), 0, 0);
   }
   authWallet_ = walletsManager_->GetAuthWallet();
   if (authWallet_) {
      connect(authWallet_.get(), &bs::Wallet::addressAdded, this, &AuthAddressManager::authAddressAdded);
   }
}

bool AuthAddressManager::setup()
{
   if (!HaveAuthWallet()) {
      addressVerificator_.reset();
      return false;
   }

   addressVerificator_ = std::make_shared<AddressVerificator>(logger_, SecureBinaryData().GenerateRandom(8).toHexStr()
      , [this](const std::shared_ptr<AuthAddress> addr, AddressVerificationState state)
   {
      if (!addressVerificator_) {
         return;
      }
      const auto address = addr->GetChainedAddress();
      if (GetState(address) != state) {
         logger_->info("Address verification {} for {}", to_string(state), address.display<std::string>());
         SetState(address, state);
         SetInitialTxHash(address, addr->GetInitialTransactionTxHash());
         SetVerifChangeTxHash(address, addr->GetVerificationChangeTxHash());
         SetBSFundingAddress(address, addr->GetBSFundingAddress());
         emit AddressListUpdated();
         if (state == AddressVerificationState::Verified) {
            emit VerifiedAddressListUpdated();
         }
      }
   });

   SetBSAddressList(bsAddressList_);
   return true;
}

void AuthAddressManager::onAuthWalletChanged()
{
   SetAuthWallet();
   addresses_.clear();
   setup();
   VerifyWalletAddresses();
   emit AuthWalletChanged();
}

AuthAddressManager::~AuthAddressManager() noexcept
{
   addressVerificator_.reset();
   FastLock lock(lockList_);
   FastLock locker(lockCommands_);
   for (auto &cmd : activeCommands_) {
      cmd->DropResult();
   }
}

size_t AuthAddressManager::GetAddressCount()
{
   FastLock locker(lockList_);
   return addresses_.size();
}

bs::Address AuthAddressManager::GetAddress(size_t index)
{
   FastLock locker(lockList_);
   if (index >= addresses_.size()) {
      return {};
   }
   return addresses_[index];
}

bool AuthAddressManager::WalletAddressesLoaded()
{
   FastLock locker(lockList_);
   return !addresses_.empty();
}

bool AuthAddressManager::IsReady() const
{
   return HasAuthAddr() && HaveBSAddressList() && ConnectedToArmory();
}

bool AuthAddressManager::HaveAuthWallet() const
{
   return (authWallet_ != nullptr);
}

bool AuthAddressManager::HasAuthAddr() const
{
   return (HaveAuthWallet() && (authWallet_->GetUsedAddressCount() > 0));
}

bool AuthAddressManager::HaveOTP() const
{
   return otpManager_->CanSign();
}

bool AuthAddressManager::NeedsOTPPassword() const
{
   return (otpManager_ && otpManager_->IsEncrypted());
}

bool AuthAddressManager::SubmitForVerification(const bs::Address &address)
{
   const auto &state = GetState(address);
   switch (state) {
   case AddressVerificationState::Verified:
   case AddressVerificationState::PendingVerification:
   case AddressVerificationState::VerificationSubmitted:
   case AddressVerificationState::Revoked:
   case AddressVerificationState::RevokedByBS:
      logger_->error("[AuthAddressManager::SubmitForVerification] refuse to submit address in state: {}", (int)state);
      return false;
   default: break;
   }

   return SubmitAddressToPublicBridge(address);
}

bool AuthAddressManager::needsOTPpassword() const
{
   return otpManager_->IsEncrypted();
}

bool AuthAddressManager::CreateNewAuthAddress()
{
   const auto &addr = authWallet_->GetNewExtAddress();
   signingContainer_->SyncAddresses({ {authWallet_, addr} });
   return true;
}

void AuthAddressManager::onTXSigned(unsigned int id, BinaryData signedTX, std::string error)
{
   const auto &itVerify = signIdsVerify_.find(id);
   const auto &itRevoke = signIdsRevoke_.find(id);
   if ((itVerify == signIdsVerify_.end()) && (itRevoke == signIdsRevoke_.end())) {
      return;
   }
   const bool isVerify = (itVerify != signIdsVerify_.end());
   signIdsVerify_.erase(id);
   signIdsRevoke_.erase(id);

   if (error.empty()) {
      if (BroadcastTransaction(signedTX)) {
         if (isVerify) {
            emit AuthVerifyTxSent();
         }
         else {
            emit AuthRevokeTxSent();
         }
      }
      else {
         emit Error(tr("Failed to broadcast transaction"));
      }
   }
   else {
      logger_->error("[AuthAddressManager::onTXSigned] TX signing failed: {}", error);
      emit Error(tr("Transaction sign error: %1").arg(QString::fromStdString(error)));
   }
}

bool AuthAddressManager::SendVerifyTransaction(const UTXO &input, uint64_t amount, const bs::Address &address
   , uint64_t remainder)
{
   auto bsFundAddr = GetBSFundingAddress(address);
   if (bsFundAddr.isNull()) {
      bsFundAddr = address;
   }

   if ((amount + remainder) >= input.getValue()) {
      logger_->error("[AuthAddressManager::SendTransaction] spend amount ({}) exceeds UTXO value ({})", amount
         , input.getValue());
      emit Error(tr("invalid TX amount"));
      return false;
   }

   const auto txReq = authWallet_->CreateTXRequest({ input }, { bsFundAddr.getRecipient(amount) }
      , input.getValue() - amount - remainder, false, address);
   const auto id = signingContainer_->SignTXRequest(txReq);
   if (id) {
      signIdsVerify_.insert(id);
      return true;
   }
   return false;
}

bool AuthAddressManager::Verify(const bs::Address &address)
{
   const auto state = GetState(address);
   if (state != AddressVerificationState::PendingVerification) {
      logger_->warn("[AuthAddressManager::Verify] attempting to verify from incorrect state {}", (int)state);
      emit Error(tr("Incorrect state"));
      return false;
   }
   if (!signingContainer_) {
      logger_->error("[AuthAddressManager::Verify] can't verify without signing container");
      emit Error(tr("Missing signing container"));
      return false;
   }

   const auto &bdm = PyBlockDataManager::instance();
   if (!bdm) {
      logger_->error("[AuthAddressManager::Verify] can't verify without Armory connection");
      emit Error(tr("Missing Armory connection"));
      return false;
   }

   UTXO verificationInput;
   for (const auto& utxo : addressVerificator_->GetVerificationInputs()) {
      if (utxo.getTxHash() == GetInitialTxHash(address)) {
         const bs::TxChecker txChecker(bdm->getTxByHash(utxo.getTxHash()));
         if ((txChecker.receiverIndex(address) != utxo.getTxOutIndex())
            || !txChecker.hasSpender(GetBSFundingAddress(address))) {
            continue;
         }
         verificationInput = utxo;
         break;
      }
   }

   if (!verificationInput.isInitialized()) {
      logger_->error("[AuthAddressManager::Verify] did not get initial tx as spendable");
      emit Error(tr("failed to find appropriate input"));
      return false;
   }

   return SendVerifyTransaction(verificationInput, addressVerificator_->GetAuthAmount(), address
      , addressVerificator_->GetAuthAmount());
}

bool AuthAddressManager::RevokeAddress(const bs::Address &address)
{
   const auto state = GetState(address);
   if ((state != AddressVerificationState::PendingVerification) && (state != AddressVerificationState::Verified)) {
      logger_->warn("[AuthAddressManager::RevokeAddress] attempting to revoke from incorrect state {}", (int)state);
      emit Error(tr("incorrect state"));
      return false;
   }
   if (!signingContainer_) {
      logger_->error("[AuthAddressManager::RevokeAddress] can't revoke without signing container");
      emit Error(tr("Missing signing container"));
      return false;
   }

   UTXO verificationChangeInput;
   for (const UTXO &tx : addressVerificator_->GetRevokeInputs()) {
      if ((tx.getTxHash() == GetVerifChangeTxHash(address))) {
         verificationChangeInput = tx;
         break;
      }
   }

   if (!verificationChangeInput.isInitialized()) {
      emit Error(tr("no appropriate input found"));
      logger_->error("[AuthAddressManager::RevokeAddress] did not get verify change tx as spendable");
      return false;
   }

   const auto &priWallet = walletsManager_->GetPrimaryWallet();
   const auto &group = priWallet->getGroup(priWallet->getXBTGroupType());
   const auto &wallet = group->getLeaf(0);
   if (!wallet) {
      emit Error(tr("no XBT wallet found"));
      logger_->error("[AuthAddressManager::RevokeAddress] XBT/0 wallet missing");
      return false;
   }
   const float feePerByte = walletsManager_->estimatedFeePerByte(3);
   const uint64_t fee = feePerByte * 135;  // magic formula for 2 inputs & 1 output, all native SW

   bs::wallet::TXMultiSignRequest txMultiReq;
   txMultiReq.addInput(verificationChangeInput, authWallet_);

   const auto &feeUTXOs = wallet->getUTXOsToSpend(fee);
   if (feeUTXOs.empty()) {
      logger_->error("[AuthAddressManager::RevokeAddress] failed to find enough UTXOs to fund {}", fee);
      emit Error(tr("Failed to find UTXOs to fund the fee"));
      return false;
   }
   uint64_t changeVal = 0;
   for (const auto &utxo : feeUTXOs) {
      txMultiReq.addInput(utxo, wallet);
      changeVal += utxo.getValue();
   }
   changeVal -= fee;

   const auto recipAddress = wallet->GetNewChangeAddress();
   const auto &recip = recipAddress.getRecipient(verificationChangeInput.getValue() + changeVal);
   if (!recip) {
      logger_->error("[AuthAddressManager::RevokeAddress] failed to create recipient");
      emit Error(tr("Failed to construct revoke transaction"));
      return false;
   }
   txMultiReq.recipients.push_back(recip);

   if (feeUTXOs.size() > 1) {
      logger_->warn("[AuthAddressManager::RevokeAddress] TX size is greater than expected ({} more inputs)", feeUTXOs.size() - 1);
      emit Info(tr("Revoke transaction size is greater than expected"));
   }

   const auto id = signingContainer_->SignMultiTXRequest(txMultiReq);
   if (id) {
      signIdsRevoke_.insert(id);
      return true;
   }
   return false;
}

void AuthAddressManager::OnDataReceived(const std::string& data)
{
   ResponsePacket response;

   if (!response.ParseFromString(data)) {
      logger_->error("[AuthAddressManager::OnDataReceived] failed to parse response from public bridge");
      return;
   }

   bool sigVerified = false;
   if (!response.has_datasignature()) {
      logger_->warn("[AuthAddressManager::OnDataReceived] Public bridge response of type {} has no signature!"
         , static_cast<int>(response.responsetype()));
   }
   else {
      BinaryData publicKey = BinaryData::CreateFromHex(settings_->get<std::string>(ApplicationSettings::bsPublicKey));
      sigVerified = CryptoECDSA().VerifyData(response.responsedata(), response.datasignature(), publicKey);
      if (!sigVerified) {
         logger_->error("[AuthAddressManager::OnDataReceived] Response signature verification failed - response {} dropped"
            , static_cast<int>(response.responsetype()));
         return;
      }
   }

   switch(response.responsetype()) {
   case RequestType::SubmitAuthAddressForVerificationType:
      ProcessSubmitAuthAddressResponse(response.responsedata(), sigVerified);
      break;
   case RequestType::GetBSFundingAddressListType:
      ProcessBSAddressListResponse(response.responsedata(), sigVerified);
      break;
   case RequestType::ErrorMessageResponseType:
      ProcessErrorResponse(response.responsedata());
      break;
   case RequestType::ConfirmAuthAddressSubmitType:
      ProcessConfirmAuthAddressSubmit(response.responsedata(), sigVerified);
      break;
   default:
      logger_->error("[AuthAddressManager::OnDataReceived] unrecognized response type from public bridge: {}", response.responsetype());
      break;
   }
}

static AddressScriptType mapToScriptType(AddressEntryType aet)
{
   switch (aet) {
   case AddressEntryType_P2SH:   return AddressScriptType::NestedSegWitScriptType;
   case AddressEntryType_P2WSH:
   case AddressEntryType_P2WPKH: return AddressScriptType::NativeSegWitScriptType;
   default:    break;
   }
   return AddressScriptType::PayToHashScriptType;
}

bool AuthAddressManager::SubmitAddressToPublicBridge(const bs::Address &address)
{
   SubmitAuthAddressForVerificationRequest addressRequest;

   addressRequest.set_username(celerClient_->userName());
   addressRequest.set_addresstype(AddressType::BitcoinsAddressType);
   addressRequest.set_networktype((settings_->get<NetworkType>(ApplicationSettings::netType) != NetworkType::MainNet)
      ? AddressNetworkType::TestNetType : AddressNetworkType::MainNetType);
   addressRequest.set_scripttype(mapToScriptType(address.getType()));

   const auto pubKey = authWallet_->GetPublicKeyFor(address);
   addressRequest.set_publickey(pubKey.toBinStr());
   addressRequest.set_address160hex(address.prefixed().toHexStr());

   RequestPacket  request;
   request.set_requesttype(SubmitAuthAddressForVerificationType);
   request.set_requestdata(addressRequest.SerializeAsString());

   logger_->debug("[AuthAddressManager::SubmitAddressToPublicBridge] submitting pubkey {}, address {} => {}", pubKey.toHexStr()
      , address.display<std::string>(), address.unprefixed().toHexStr());

   return SubmitRequestToPB("submit_address", request.SerializeAsString());
}

bool AuthAddressManager::ConfirmSubmitForVerification(const bs::Address &address, const SecureBinaryData &otpPassword)
{
   ConfirmAuthSubmitRequest request;

   request.set_username(celerClient_->userName());
   request.set_address(address.display<std::string>());
   request.set_publickey(authWallet_->GetPublicKeyFor(address).toBinStr());
   request.set_networktype((settings_->get<NetworkType>(ApplicationSettings::netType) != NetworkType::MainNet)
      ? AddressNetworkType::TestNetType : AddressNetworkType::MainNetType);
   request.set_scripttype(mapToScriptType(address.getType()));
   const auto &data = request.SerializeAsString();

   RequestPacket  packet;

   const auto cbSigned = [&packet](const SecureBinaryData &sig, const std::string &otpId, unsigned int keyIndex) {
      packet.set_datasignature(sig.toBinStr());
      packet.set_otpid(otpId);
      packet.set_keyindex(keyIndex);
   };

   if (!otpManager_->Sign(data, otpPassword, cbSigned)) {
      logger_->debug("[AuthAddressManager::ConfirmSubmitForVerification] failed to OTP sign data");
      emit OtpSignFailed();
      return false;
   }

   packet.set_requesttype(ConfirmAuthAddressSubmitType);
   packet.set_requestdata(data);

   logger_->debug("[AuthAddressManager::ConfirmSubmitForVerification] confirmed auth address submission");
   return SubmitRequestToPB("confirm_submit_auth_addr", packet.SerializeAsString());
}

bool AuthAddressManager::CancelSubmitForVerification(const bs::Address &address)
{
   ConfirmAuthSubmitRequest request;

   request.set_username(celerClient_->userName());
   request.set_address(address.display<std::string>());
   request.set_publickey("");
   request.set_networktype((settings_->get<NetworkType>(ApplicationSettings::netType) != NetworkType::MainNet)
      ? AddressNetworkType::TestNetType : AddressNetworkType::MainNetType);
   request.set_scripttype(mapToScriptType(address.getType()));
   const auto &data = request.SerializeAsString();

   RequestPacket  packet;

   packet.set_requesttype(ConfirmAuthAddressSubmitType);
   packet.set_requestdata(data);

   logger_->debug("[AuthAddressManager::CancelSubmitForVerification] confirmed auth address submission");
   return SubmitRequestToPB("confirm_submit_auth_addr", packet.SerializeAsString());
}

AddressEntryType AuthAddressManager::mapFromScriptType(AddressScriptType scrType)
{
   switch (scrType) {
   case AddressScriptType::NestedSegWitScriptType: return AddressEntryType_P2SH;
   case AddressScriptType::NativeSegWitScriptType: return AddressEntryType_P2WPKH;
   default:    break;
   }
   return AddressEntryType_P2PKH;
}

void AuthAddressManager::SubmitToCeler(const bs::Address &address)
{
   if (celerClient_->IsConnected()) {
      const std::string addressString = address.display<std::string>();
      std::unordered_set<std::string> submittedAddresses = celerClient_->GetSubmittedAuthAddressSet();
      if (submittedAddresses.find(addressString) == submittedAddresses.end()) {
         submittedAddresses.emplace(addressString);
         celerClient_->SetSubmittedAuthAddressSet(submittedAddresses);
      }
   }
   else {
      logger_->debug("[AuthAddressManager::SubmitToCeler] Celer is not connected");
   }
}

void AuthAddressManager::ProcessSubmitAuthAddressResponse(const std::string& responseString, bool)
{
   SubmitAuthAddressForVerificationResponse response;
   if (!response.ParseFromString(responseString)) {
      logger_->error("[AuthAddressManager::ProcessSubmitAuthAddressResponse] failed to parse response");
      return;
   }

   const bs::Address address(BinaryData::CreateFromHex(response.address160hex()), mapFromScriptType(response.scripttype()));
   if (response.keysubmitted()) {
      if (response.requestconfirmation()) {
         emit AuthAddressConfirmationRequired(response.validationamount());
      } else {
         logger_->debug("[AuthAddressManager::ProcessSubmitAuthAddressResponse] address submitted. No erification required");
      }
   }
   else {
      if (response.has_errormessage()) {
         logger_->error("[AuthAddressManager::ProcessSubmitAuthAddressResponse] auth address {} rejected: {}"
            , BinaryData(response.address160hex()).toHexStr(), response.errormessage());
         emit Error(tr("Authentication Address rejected: %1").arg(QString::fromStdString(response.errormessage())));
      } else {
         logger_->error("[AuthAddressManager::ProcessSubmitAuthAddressResponse] auth address {} rejected"
            , BinaryData(response.address160hex()).toHexStr());
         emit Error(tr("Authentication Address rejected"));
      }
   }
}

void AuthAddressManager::ProcessConfirmAuthAddressSubmit(const std::string &responseData, bool)
{
   ConfirmAuthSubmitResponse response;
   if (!response.ParseFromString(responseData)) {
      logger_->error("[AuthAddressManager::ProcessConfirmAuthAddressSubmit] failed to parse response");
      return;
   }

   const bs::Address address(response.address());
   if (response.has_errormsg()) {
      emit AuthAddrSubmitError(address.display(), QString::fromStdString(response.errormsg()));
   }
   else {
      SubmitToCeler(address);
      SetState(address, AddressVerificationState::Submitted);
      emit AddressListUpdated();
      emit AuthAddrSubmitSuccess(address.display());
   }
}

void AuthAddressManager::ProcessErrorResponse(const std::string& responseString) const
{
   ErrorMessageResponse response;
   if (!response.ParseFromString(responseString)) {
      logger_->error("[AuthAddressManager::ProcessErrorResponse] failed to parse error message response");
      return;
   }

   logger_->error("[AuthAddressManager::ProcessErrorResponse] error message from public bridge: {}", response.errormessage());
}

void AuthAddressManager::VerifyWalletAddresses()
{
   if (IsReady()) {
      VerifyWalletAddressesFunction();
   }
}

void AuthAddressManager::VerifyWalletAddressesFunction()
{
   if (!HaveBSAddressList()) {
      return;
   }
   bool updated = false;

   const auto &submittedAddresses = celerClient_->GetSubmittedAuthAddressSet();

   if (!WalletAddressesLoaded()) {
      if (authWallet_ != nullptr) {
         for (const auto &addr : authWallet_->GetUsedAddressList()) {
            AddAddress(addr);
            if (submittedAddresses.find(addr.display<std::string>()) != submittedAddresses.end()) {
               SetState(addr, AddressVerificationState::Submitted);
            }
         }
      }
      updated = true;

      auto defaultAuthAddrStr = settings_->get<QString>(ApplicationSettings::defaultAuthAddr);
      if (!defaultAuthAddrStr.isEmpty()) {
         defaultAddr_ = bs::Address(defaultAuthAddrStr);
      }

      if (defaultAddr_.isNull()) {
         logger_->debug("Default auth address not found");
      }
      else {
         logger_->debug("Default auth address: {}", defaultAddr_.display<std::string>());
      }
   }

   std::vector<bs::Address> listCopy;
   {
      FastLock locker(lockList_);
      listCopy = addresses_;
   }

   for (auto &addr : listCopy) {
      addressVerificator_->StartAddressVerification(std::make_shared<AuthAddress>(addr));
   }
   addressVerificator_->RegisterBSAuthAddresses();
   addressVerificator_->RegisterAddresses();

   if (updated) {
      emit VerifiedAddressListUpdated();
      emit AddressListUpdated();
   }
}

void AuthAddressManager::OnDisconnectedFromCeler()
{
   ClearAddressList();
}

void AuthAddressManager::ClearAddressList()
{
   bool adressListChanged = false;
   {
      FastLock locker(lockList_);
      if (!addresses_.empty()) {
         addresses_.clear();
         adressListChanged = true;
      }
   }

   if (adressListChanged) {
      emit AddressListUpdated();
      emit VerifiedAddressListUpdated();
   }
}

void AuthAddressManager::authAddressAdded()
{
   bool listUpdated = false;
   if (authWallet_ != nullptr) {
      const auto &newAddresses = authWallet_->GetUsedAddressList();
      const auto count = newAddresses.size();
      listUpdated = (count > addresses_.size());

      for (size_t i = addresses_.size(); i < count; i++) {
         const auto &addr = newAddresses[i];
         AddAddress(addr);
         const auto authAddr = std::make_shared<AuthAddress>(addr);
         addressVerificator_->StartAddressVerification(authAddr);
      }
   }

   if (listUpdated) {
      emit AddressListUpdated();
      addressVerificator_->RegisterAddresses();
      authWallet_->RegisterWallet();
   }
}

void AuthAddressManager::AddAddress(const bs::Address &addr)
{
   SetState(addr, AddressVerificationState::InProgress);
   FastLock locker(lockList_);
   addresses_.emplace_back(addr);
}

bool AuthAddressManager::HaveBSAddressList() const
{
   return !bsAddressList_.empty();
}

const std::unordered_set<std::string> &AuthAddressManager::GetBSAddresses() const
{
   return bsAddressList_;
}

bool AuthAddressManager::ConnectedToArmory() const
{
   return addressVerificator_ != nullptr;
}

bool AuthAddressManager::SendGetBSAddressListRequest()
{
   GetBSFundingAddressListRequest addressRequest;
   RequestPacket  request;

   addressRequest.set_addresslisttype(BitcoinsAddressType);

   request.set_requesttype(GetBSFundingAddressListType);
   request.set_requestdata(addressRequest.SerializeAsString());

   const bool rc = SubmitRequestToPB("get_bs_list", request.SerializeAsString());
   if (!rc) {
      emit ConnectionComplete();
   }
   return rc;
}

bool AuthAddressManager::SubmitRequestToPB(const std::string& name, const std::string& data)
{
   const auto connection = connectionManager_->CreateSecuredDataConnection();
   connection->SetServerPublicKey(settings_->get<std::string>(ApplicationSettings::pubBridgePubKey));
   auto command = std::make_shared<RequestReplyCommand>(name, connection, logger_);

   command->SetReplyCallback([command, this](const std::string& data) {
      OnDataReceived(data);
      command->SetReplyCallback(nullptr);
      FastLock locker(lockCommands_);
      activeCommands_.erase(command);
      return true;
   });

   command->SetErrorCallback([command, this](const std::string& message) {
      logger_->error("[AuthAddressManager::{}] error callback: {}", command->GetName(), message);
      command->SetReplyCallback(nullptr);
      FastLock locker(lockCommands_);
      activeCommands_.erase(command);
   });

   {
      FastLock locker(lockCommands_);
      activeCommands_.insert(command);
   }

   if (!command->ExecuteRequest(settings_->get<std::string>(ApplicationSettings::pubBridgeHost)
         , settings_->get<std::string>(ApplicationSettings::pubBridgePort)
         , data))
   {
      logger_->error("[AuthAddressManager::SubmitRequestToPB] failed to send request {}", name);
      FastLock locker(lockCommands_);
      activeCommands_.erase(command);
      return false;
   }

   return true;
}

void AuthAddressManager::ProcessBSAddressListResponse(const std::string& response, bool sigVerified)
{
   GetBSFundingAddressListResponse recvList;

   if (!recvList.ParseFromString(response)) {
      logger_->error("[AuthAddressManager::ProcessBSAddressListResponse] data corrupted. Could not parse.");
      return;
   }
   if (!sigVerified) {
      logger_->error("[AuthAddressManager::ProcessBSAddressListResponse] rejecting unverified response");
      return;
   }

   if (recvList.addresslisttype() != AddressType::BitcoinsAddressType) {
      logger_->error("[AuthAddressManager::ProcessBSAddressListResponse] invalid address list type: {}"
         , recvList.addresslisttype());
      return;
   }

   std::unordered_set<std::string> tempList;
   int size = recvList.addresslist_size();
   for (int i = 0; i < size; i++) {
      tempList.emplace(recvList.addresslist(i));
   }

   logger_->debug("[AuthAddressManager::ProcessBSAddressListResponse] get {} BS addresses", tempList.size());

   ClearAddressList();
   SetBSAddressList(tempList);
   VerifyWalletAddresses();
   emit ConnectionComplete();
}

AddressVerificationState AuthAddressManager::GetState(const bs::Address &addr) const
{
   const auto itState = states_.find(addr.prefixed());
   if (itState == states_.end()) {
      return AddressVerificationState::VerificationFailed;
   }
   return itState->second;
}

void AuthAddressManager::SetState(const bs::Address &addr, AddressVerificationState state)
{
   const auto prevState = GetState(addr);
   if ((prevState == AddressVerificationState::Submitted) && (state == AddressVerificationState::NotSubmitted)) {
      return;
   }
   states_[addr.prefixed()] = state;

   if (state == AddressVerificationState::PendingVerification) {
      emit NeedVerify(addr.display());
   }
   else if ((state == AddressVerificationState::Verified) && (prevState == AddressVerificationState::VerificationSubmitted)) {
      emit AddrStateChanged(addr.display(), tr("Verified"));
   }
   else if (((state == AddressVerificationState::Revoked) || (state == AddressVerificationState::RevokedByBS))
      && (prevState == AddressVerificationState::Verified)) {
      emit AddrStateChanged(addr.display(), tr("Revoked"));
   }
}

bool AuthAddressManager::BroadcastTransaction(const BinaryData& transactionData)
{
   return PyBlockDataManager::instance()->broadcastZC(transactionData);
}

BinaryData AuthAddressManager::GetPublicKey(size_t index)
{
   return authWallet_->GetPubChainedKeyFor(GetAddress(index));
}

void AuthAddressManager::setDefault(const bs::Address &addr)
{
   defaultAddr_ = addr;
   emit VerifiedAddressListUpdated();
}

size_t AuthAddressManager::getDefaultIndex() const
{
   if (defaultAddr_.isNull()) {
      return 0;
   }
   size_t rv = 0;
   FastLock locker(lockList_);
   for (const auto& address : addresses_) {
      if (GetState(address) != AddressVerificationState::Verified) {
         continue;
      }
      if (address.prefixed() == defaultAddr_.prefixed()) {
         return rv;
      }
      rv++;
   }
   return 0;
}

std::vector<bs::Address> AuthAddressManager::GetVerifiedAddressList() const
{
   std::vector<bs::Address> list;
   {
      FastLock locker(lockList_);
      for (const auto& address : addresses_) {
         if (GetState(address) == AddressVerificationState::Verified) {
            list.emplace_back(address);
         }
      }
   }
   return list;
}

size_t AuthAddressManager::FromVerifiedIndex(size_t index) const
{
   if (index < addresses_.size()) {
      size_t nbVerified = 0;
      for (size_t i = 0; i < addresses_.size(); i++) {
         if (GetState(addresses_[i]) == AddressVerificationState::Verified) {
            if (nbVerified == index) {
               return i;
            }
            nbVerified++;
         }
      }
   }
   return UINT32_MAX;
}

void AuthAddressManager::SetBSAddressList(const std::unordered_set<std::string>& bsAddressList)
{
   FastLock locker(lockList_);
   bsAddressList_ = bsAddressList;

   if (!bsAddressList.empty()) {
      if (addressVerificator_) {
         addressVerificator_->SetBSAddressList(bsAddressList);
      }
   }
}

template <typename TVal> TVal AuthAddressManager::lookup(const bs::Address &key, const std::map<bs::Address, TVal> &container) const
{
   const auto it = container.find(key);
   if (it == container.end()) {
      return TVal();
   }
   return it->second;
}

void AuthAddressManager::CreateAuthWallet(const SecureBinaryData &password, bool signal)
{
   if (!signingContainer_ || !walletsManager_) {
      emit Error(tr("Unable to create auth wallet"));
      return;
   }
   const auto &priWallet = walletsManager_->GetPrimaryWallet();
   if (!priWallet) {
      emit Error(tr("Primary wallet doesn't exist"));
      return;
   }
   const auto &authGrp = priWallet->getGroup(bs::hd::CoinType::BlockSettle_Auth);
   if (authGrp->getLeaf(0u) != nullptr) {
      emit Error(tr("Authentication wallet already exists"));
      return;
   }
   bs::hd::Path path;
   path.append(bs::hd::purpose, true);
   path.append(bs::hd::CoinType::BlockSettle_Auth, true);
   path.append(0u, true);
   createWalletReqId_ = { signingContainer_->CreateHDLeaf(priWallet, path, password), signal };
}

void AuthAddressManager::onWalletCreated(unsigned int id, BinaryData pubKey, BinaryData chainCode, std::string walletId)
{
   if (!createWalletReqId_.first || (createWalletReqId_.first != id)) {
      return;
   }
   const auto &priWallet = walletsManager_->GetPrimaryWallet();
   const auto &leafNode = std::make_shared<bs::hd::Node>(pubKey, chainCode, priWallet->networkType());
   const auto &group = priWallet->getGroup(bs::hd::CoinType::BlockSettle_Auth);
   const auto leaf = group->createLeaf(0u, leafNode);
   if (leaf) {
      if (createWalletReqId_.second) {
         emit AuthWalletCreated(QString::fromStdString(leaf->GetWalletId()));
      }
      emit walletsManager_->walletChanged();
   }
   else {
      emit Error(tr("Failed to create auth subwallet"));
   }
   createWalletReqId_ = { 0, true };
}

void AuthAddressManager::onWalletFailed(unsigned int id, std::string errMsg)
{
   if (!createWalletReqId_.first || (createWalletReqId_.first != id)) {
      return;
   }
   createWalletReqId_ = { 0, true };
   emit Error(tr("Failed to create auth subwallet: %1").arg(QString::fromStdString(errMsg)));
}
