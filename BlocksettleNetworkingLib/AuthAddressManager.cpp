#include "AuthAddressManager.h"
#include <spdlog/spdlog.h>
#include <memory>
#include <QtConcurrent/QtConcurrentRun>
#include "AddressVerificator.h"
#include "ApplicationSettings.h"
#include "ArmoryConnection.h"
#include "AuthSignManager.h"
#include "CelerClient.h"
#include "CheckRecipSigner.h"
#include "ClientClasses.h"
#include "ConnectionManager.h"
#include "FastLock.h"
#include "RequestReplyCommand.h"
#include "SignContainer.h"
#include "Wallets/SyncHDWallet.h"
#include "Wallets/SyncWalletsManager.h"
#include "ZMQ_BIP15X_DataConnection.h"

using namespace Blocksettle::Communication;


AuthAddressManager::AuthAddressManager(const std::shared_ptr<spdlog::logger> &logger
   , const std::shared_ptr<ArmoryConnection> &armory
   , const ZmqBIP15XDataConnection::cbNewKey &cb)
   : QObject(nullptr), logger_(logger), armory_(armory), cbApproveConn_(cb)
{}

void AuthAddressManager::init(const std::shared_ptr<ApplicationSettings>& appSettings
   , const std::shared_ptr<bs::sync::WalletsManager> &walletsManager
   , const std::shared_ptr<AuthSignManager> &authSignManager
   , const std::shared_ptr<SignContainer> &container)
{
   settings_ = appSettings;
   walletsManager_ = walletsManager;
   authSignManager_ = authSignManager;
   signingContainer_ = container;

   connect(walletsManager_.get(), &bs::sync::WalletsManager::blockchainEvent, this, &AuthAddressManager::VerifyWalletAddresses);
   connect(walletsManager_.get(), &bs::sync::WalletsManager::authWalletChanged, this, &AuthAddressManager::onAuthWalletChanged);
   connect(walletsManager_.get(), &bs::sync::WalletsManager::walletChanged, this, &AuthAddressManager::onWalletChanged);

   // signingContainer_ might be null if user rejects remote signer key
   if (signingContainer_) {
      connect(signingContainer_.get(), &SignContainer::TXSigned, this, &AuthAddressManager::onTXSigned);
      connect(signingContainer_.get(), &SignContainer::Error, this, &AuthAddressManager::onWalletFailed);
      connect(signingContainer_.get(), &SignContainer::HDLeafCreated, this, &AuthAddressManager::onWalletCreated);
   }

   SetAuthWallet();
}

void AuthAddressManager::ConnectToPublicBridge(const std::shared_ptr<ConnectionManager> &connMgr
   , const std::shared_ptr<BaseCelerClient>& celerClient)
{
   connectionManager_ = connMgr;
   celerClient_ = celerClient;

   QtConcurrent::run(this, &AuthAddressManager::SendGetBSAddressListRequest);
}

void AuthAddressManager::SetAuthWallet()
{
   authWallet_ = walletsManager_->getAuthWallet();
}

bool AuthAddressManager::setup()
{
   if (!HaveAuthWallet()) {
      logger_->debug("Auth wallet missing");
      addressVerificator_.reset();
      return false;
   }
   if (addressVerificator_) {
      return true;
   }

   addressVerificator_ = std::make_shared<AddressVerificator>(logger_, armory_, CryptoPRNG::generateRandom(8).toHexStr()
      , [this](const std::shared_ptr<AuthAddress> addr, AddressVerificationState state)
   {
      if (!addressVerificator_) {
         return;
      }
      const auto address = addr->GetChainedAddress();
      if (GetState(address) != state) {
         logger_->info("Address verification {} for {}", to_string(state), address.display());
         //FIXME: temp disabled for simulating address verification
         //SetState(address, state);
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

AuthAddressManager::~AuthAddressManager() noexcept = default;

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
   return HasAuthAddr() && HaveBSAddressList() && armory_ && armory_->isOnline();
}

bool AuthAddressManager::HaveAuthWallet() const
{
   return (authWallet_ != nullptr);
}

bool AuthAddressManager::HasAuthAddr() const
{
   return (HaveAuthWallet() && (authWallet_->getUsedAddressCount() > 0));
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

bool AuthAddressManager::CreateNewAuthAddress()
{
   const auto &cbAddr = [this](const bs::Address &) {
      emit walletsManager_->walletChanged(authWallet_->walletId());
   };
   authWallet_->getNewExtAddress(cbAddr);
   return true;
}

void AuthAddressManager::onTXSigned(unsigned int id, BinaryData signedTX, bs::error::ErrorCode result, const std::string &errorReason)
{
   const auto &itVerify = signIdsVerify_.find(id);
   const auto &itRevoke = signIdsRevoke_.find(id);
   if ((itVerify == signIdsVerify_.end()) && (itRevoke == signIdsRevoke_.end())) {
      return;
   }
   const bool isVerify = (itVerify != signIdsVerify_.end());
   signIdsVerify_.erase(id);
   signIdsRevoke_.erase(id);

   if (result == bs::error::ErrorCode::NoError) {
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
      logger_->error("[AuthAddressManager::onTXSigned] TX signing failed: {} {}"
         , bs::error::ErrorCodeToString(result).toStdString(), errorReason);
      emit Error(tr("Transaction sign error: %1").arg(bs::error::ErrorCodeToString(result)));
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

   const auto txReq = authWallet_->createTXRequest({ input }, { bsFundAddr.getRecipient(amount) }
      , input.getValue() - amount - remainder, false, address);
   const auto id = signingContainer_->signTXRequest(txReq);
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

   if (!armory_ || (armory_->state() != ArmoryState::Ready)) {
      logger_->error("[AuthAddressManager::Verify] can't verify without Armory connection");
      emit Error(tr("Missing Armory connection"));
      return false;
   }

   const auto &cbInputs = [this, address](const std::vector<UTXO> &inputs) {
      std::set<BinaryData> txHashSet;
      std::vector<UTXO> utxos;
      const auto &initialTxHash = GetInitialTxHash(address);
      for (const auto &utxo : inputs) {
         if (utxo.getTxHash() == initialTxHash) {
            txHashSet.insert(utxo.getTxHash());
            utxos.emplace_back(std::move(utxo));
         }
      }
      const auto &cbTXs = [this, address, utxos](const std::vector<Tx> &txs) {
         for (const auto &tx : txs) {
            const bs::TxChecker txChecker(tx);
            for (const auto &utxo : utxos) {
               if (txChecker.receiverIndex(address) == utxo.getTxOutIndex()) {
                     const auto &cbSpender = [this, address, utxo](bool present) {
                     if (!present) {
                        return;
                     }
                     SendVerifyTransaction(utxo, addressVerificator_->GetAuthAmount()
                        , address, addressVerificator_->GetAuthAmount());
                  };
                  txChecker.hasSpender(GetBSFundingAddress(address), armory_, cbSpender);
               }
            }
         }
      };
      if (txHashSet.empty()) {
         emit Error(tr("Invalid initial TX"));
      }
      else {
         armory_->getTXsByHash(txHashSet, cbTXs);
      }
   };
   addressVerificator_->GetVerificationInputs(cbInputs);

   return true;
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

   const auto &cbInputs = [this, address](std::vector<UTXO> inputs) {
      UTXO verificationChangeInput;
      for (const auto &tx : inputs) {
         if ((tx.getTxHash() == GetVerifChangeTxHash(address))) {
            verificationChangeInput = tx;
            break;
         }
      }
      if (!verificationChangeInput.isInitialized()) {
         emit Error(tr("no appropriate input found"));
         logger_->error("[AuthAddressManager::RevokeAddress] did not get verify change tx as spendable");
         return;
      }

      const auto &cbFee = [this, verificationChangeInput](float feePerByte) {
         const auto &priWallet = walletsManager_->getPrimaryWallet();
         const auto &group = priWallet->getGroup(priWallet->getXBTGroupType());
         const auto &wallet = group->getLeaf(0);
         if (!wallet) {
            emit Error(tr("no XBT wallet found"));
            logger_->error("[AuthAddressManager::RevokeAddress] XBT/0 wallet missing");
            return;
         }

         const uint64_t fee = feePerByte * 135;  // magic formula for 2 inputs & 1 output, all native SW

         auto txMultiReq = new bs::core::wallet::TXMultiSignRequest;
         txMultiReq->addInput(verificationChangeInput, authWallet_->walletId());

         const auto &cbFeeUTXOs = [this, fee, txMultiReq, wallet](std::vector<UTXO> utxos) {
            if (utxos.empty()) {
               logger_->error("[AuthAddressManager::RevokeAddress] failed to find enough UTXOs to fund {}", fee);
               emit Error(tr("Failed to find UTXOs to fund the fee"));
               return;
            }
            uint64_t changeVal = 0;
            for (const auto &utxo : utxos) {
               txMultiReq->addInput(utxo, wallet->walletId());
               changeVal += utxo.getValue();
            }
            changeVal -= fee;

            const auto &cbRecipAddr = [this, txMultiReq, changeVal, utxos](const bs::Address &recipAddress) {
               const auto &recip = recipAddress.getRecipient(txMultiReq->inputs.cbegin()->first.getValue() + changeVal);
               if (!recip) {
                  logger_->error("[AuthAddressManager::RevokeAddress] failed to create recipient");
                  emit Error(tr("Failed to construct revoke transaction"));
                  return;
               }
               txMultiReq->recipients.push_back(recip);

               if (utxos.size() > 1) {
                  logger_->warn("[AuthAddressManager::RevokeAddress] TX size is greater than expected ({} more inputs)", utxos.size() - 1);
                  emit Info(tr("Revoke transaction size is greater than expected"));
               }

               const auto id = signingContainer_->signMultiTXRequest(*txMultiReq);
               if (id) {
                  signIdsRevoke_.insert(id);
               }
            };
            wallet->getNewChangeAddress(cbRecipAddr);
         };
         wallet->getSpendableTxOutList(cbFeeUTXOs, fee);
      };
      walletsManager_->estimatedFeePerByte(3, cbFee, this);
   };
   addressVerificator_->GetRevokeInputs(cbInputs);
   return true;
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
   case RequestType::CancelAuthAddressSubmitType:
      ProcessCancelAuthSubmitResponse(response.responsedata());
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

   addressRequest.set_address160hex(address.prefixed().toHexStr());

   RequestPacket  request;
   request.set_requesttype(SubmitAuthAddressForVerificationType);
   request.set_requestdata(addressRequest.SerializeAsString());

   logger_->debug("[AuthAddressManager::SubmitAddressToPublicBridge] submitting address {} => {}"
      , address.display(), address.unprefixed().toHexStr());

   return SubmitRequestToPB("submit_address", request.SerializeAsString());
}

bool AuthAddressManager::ConfirmSubmitForVerification(const bs::Address &address, int expireTimeoutSeconds)
{
   ConfirmAuthSubmitRequest request;

   request.set_username(celerClient_->userName());
   request.set_address(address.display());
   request.set_networktype((settings_->get<NetworkType>(ApplicationSettings::netType) != NetworkType::MainNet)
      ? AddressNetworkType::TestNetType : AddressNetworkType::MainNetType);
   request.set_scripttype(mapToScriptType(address.getType()));
   request.set_userid(celerClient_->userId());

   std::string requestData = request.SerializeAsString();
   BinaryData requestDataHash = BtcUtils::getSha256(requestData);

   const auto cbSigned = [this, requestData](const AutheIDClient::SignResult &result) {
      RequestPacket  packet;

      packet.set_requesttype(ConfirmAuthAddressSubmitType);
      packet.set_requestdata(requestData);

      // Copy AuthEid signature
      auto autheidSign = packet.mutable_autheidsign();
      autheidSign->set_serialization(AuthEidSign::Serialization(result.serialization));
      autheidSign->set_signature_data(result.data.toBinStr());
      autheidSign->set_sign(result.sign.toBinStr());
      autheidSign->set_certificate_client(result.certificateClient.toBinStr());
      autheidSign->set_certificate_issuer(result.certificateIssuer.toBinStr());
      autheidSign->set_ocsp_response(result.ocspResponse.toBinStr());

      logger_->debug("[AuthAddressManager::ConfirmSubmitForVerification] confirmed auth address submission");
      SubmitRequestToPB("confirm_submit_auth_addr", packet.SerializeAsString());
   };

   const auto cbSignFailed = [this](const QString &text) {
      logger_->error("[AuthAddressManager::ConfirmSubmitForVerification] failed to sign data: {}", text.toStdString());
      emit SignFailed(text);
   };

   return authSignManager_->Sign(requestDataHash, tr("Authentication Address")
      , tr("Submit auth address for verification"), cbSigned, cbSignFailed, expireTimeoutSeconds);
}

bool AuthAddressManager::CancelSubmitForVerification(const bs::Address &address)
{
   CancelAuthAddressSubmitRequest request;

   request.set_username(celerClient_->userName());
   request.set_address(address.display());
   request.set_userid(celerClient_->userId());

   RequestPacket  packet;

   packet.set_requesttype(CancelAuthAddressSubmitType);
   packet.set_requestdata(request.SerializeAsString());

   logger_->debug("[AuthAddressManager::CancelSubmitForVerification] cancel submission of {}"
      , address.display());

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
      const std::string addressString = address.display();
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
         logger_->debug("[AuthAddressManager::ProcessSubmitAuthAddressResponse] address submitted. No verification required");
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
      emit AuthAddrSubmitError(QString::fromStdString(address.display()), QString::fromStdString(response.errormsg()));
   }
   else {
      SubmitToCeler(address);
      SetState(address, AddressVerificationState::Submitted);
      emit AddressListUpdated();
      emit AuthAddrSubmitSuccess(QString::fromStdString(address.display()));
   }
}

void AuthAddressManager::ProcessCancelAuthSubmitResponse(const std::string& responseData)
{
   CancelAuthAddressSubmitResponse response;
   if (!response.ParseFromString(responseData)) {
      logger_->error("[AuthAddressManager::ProcessCancelAuthSubmitResponse] failed to parse response");
      return;
   }

   const bs::Address address(response.address());
   if (response.has_errormsg()) {

   } else {
      SetState(address, AddressVerificationState::NotSubmitted);
      emit AddressListUpdated();
      emit AuthAddressSubmitCancelled(QString::fromStdString(address.display()));
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
      logger_->debug("AuthAddressManager doesn't have BS addresses");
      return;
   }
   bool updated = false;

   const auto &submittedAddresses = celerClient_->GetSubmittedAuthAddressSet();

   if (!WalletAddressesLoaded()) {
      if (authWallet_ != nullptr) {
         for (const auto &addr : authWallet_->getUsedAddressList()) {
            AddAddress(addr);
            if (submittedAddresses.find(addr.display()) != submittedAddresses.end()) {
               SetState(addr, AddressVerificationState::Submitted);
            }
         }
      }
      else {
         logger_->debug("AuthAddressManager auth wallet is null");
      }
      updated = true;

      auto defaultAuthAddrStr = settings_->get<QString>(ApplicationSettings::defaultAuthAddr);
      if (!defaultAuthAddrStr.isEmpty()) {
         defaultAddr_ = bs::Address(defaultAuthAddrStr.toStdString());
      }

      if (defaultAddr_.isNull()) {
         logger_->debug("Default auth address not found");
      }
      else {
         logger_->debug("Default auth address: {}", defaultAddr_.display());
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

void AuthAddressManager::onWalletChanged(const std::string &walletId)
{
   bool listUpdated = false;
   if ((authWallet_ != nullptr) && (walletId == authWallet_->walletId())) {
      const auto &newAddresses = authWallet_->getUsedAddressList();
      const auto count = newAddresses.size();
      listUpdated = (count > addresses_.size());

      //FIXME: temporary code to simulate address verification
      listUpdated = true;
      addresses_ = newAddresses;
      for (const auto &addr : newAddresses) {
         SetState(addr, AddressVerificationState::Verified);
      }
      emit VerifiedAddressListUpdated();

      // FIXME: address verification is disabled temporarily
/*      for (size_t i = addresses_.size(); i < count; i++) {
         const auto &addr = newAddresses[i];
         AddAddress(addr);
         const auto authAddr = std::make_shared<AuthAddress>(addr);
         addressVerificator_->StartAddressVerification(authAddr);
      }*/
   }

   if (listUpdated) {
      emit AddressListUpdated();
//      addressVerificator_->RegisterAddresses();  //FIXME: re-enable later
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
   auto connection = connectionManager_->CreateZMQBIP15XDataConnection();
   connection->setCBs(cbApproveConn_);

   requestId_ += 1;
   int requestId = requestId_;

   auto command = std::make_unique<RequestReplyCommand>(name, connection, logger_);

   command->SetReplyCallback([requestId, this](const std::string& data) {
      OnDataReceived(data);

      QMetaObject::invokeMethod(this, [this, requestId] {
         activeCommands_.erase(requestId);
      });
      return true;
   });

   command->SetErrorCallback([requestId, this](const std::string& message) {
      QMetaObject::invokeMethod(this, [this, requestId, message] {
         auto it = activeCommands_.find(requestId);
         if (it != activeCommands_.end()) {
            logger_->error("[AuthAddressManager::{}] error callback: {}", it->second->GetName(), message);
            activeCommands_.erase(it);
         }
      });
   });

   if (!command->ExecuteRequest(settings_->pubBridgeHost(), settings_->pubBridgePort()
         , data, true)) {
      logger_->error("[AuthAddressManager::SubmitRequestToPB] failed to send request {}", name);
      return false;
   }

   activeCommands_.emplace(requestId, std::move(command));

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
      emit NeedVerify(QString::fromStdString(addr.display()));
   }
   else if ((state == AddressVerificationState::Verified) && (prevState == AddressVerificationState::VerificationSubmitted)) {
      emit AddrStateChanged(QString::fromStdString(addr.display()), tr("Verified"));
   }
   else if (((state == AddressVerificationState::Revoked) || (state == AddressVerificationState::RevokedByBS))
      && (prevState == AddressVerificationState::Verified)) {
      emit AddrStateChanged(QString::fromStdString(addr.display()), tr("Revoked"));
   }
}

bool AuthAddressManager::BroadcastTransaction(const BinaryData& transactionData)
{
   return armory_->broadcastZC(transactionData);
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

void AuthAddressManager::CreateAuthWallet(const std::vector<bs::wallet::PasswordData> &pwdData, bool signal)
{
   if (!signingContainer_ || !walletsManager_) {
      emit Error(tr("Unable to create auth wallet"));
      return;
   }
   const auto &priWallet = walletsManager_->getPrimaryWallet();
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
   path.append(bs::hd::purpose | 0x80000000);
   path.append(bs::hd::CoinType::BlockSettle_Auth | 0x80000000);
   path.append(0x80000000);
   createWalletReqId_ = { signingContainer_->createHDLeaf(priWallet->walletId(), path, pwdData), signal };
}

void AuthAddressManager::onWalletCreated(unsigned int id, const std::shared_ptr<bs::sync::hd::Leaf> &leaf)
{
   if (!createWalletReqId_.first || (createWalletReqId_.first != id)) {
      return;
   }
   createWalletReqId_ = { 0, true };

   const auto &priWallet = walletsManager_->getPrimaryWallet();
   const auto &group = priWallet->getGroup(bs::hd::CoinType::BlockSettle_Auth);
   group->addLeaf(leaf);

   if (createWalletReqId_.second) {
      emit AuthWalletCreated(QString::fromStdString(leaf->walletId()));
   }
   emit walletsManager_->walletChanged(leaf->walletId());
}

void AuthAddressManager::onWalletFailed(unsigned int id, std::string errMsg)
{
   if (!createWalletReqId_.first || (createWalletReqId_.first != id)) {
      return;
   }
   createWalletReqId_ = { 0, true };
   emit Error(tr("Failed to create auth subwallet: %1").arg(QString::fromStdString(errMsg)));
}
