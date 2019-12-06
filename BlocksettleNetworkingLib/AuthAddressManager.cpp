/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "AuthAddressManager.h"
#include <spdlog/spdlog.h>
#include <memory>
#include <QtConcurrent/QtConcurrentRun>
#include "AddressVerificator.h"
#include "ApplicationSettings.h"
#include "ArmoryConnection.h"
#include "BsClient.h"
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
   , const ZmqBipNewKeyCb &cb)
   : QObject(nullptr), logger_(logger), armory_(armory), cbApproveConn_(cb)
{}

void AuthAddressManager::init(const std::shared_ptr<ApplicationSettings>& appSettings
   , const std::shared_ptr<bs::sync::WalletsManager> &walletsManager
   , const std::shared_ptr<SignContainer> &container)
{
   settings_ = appSettings;
   walletsManager_ = walletsManager;
   signingContainer_ = container;

   connect(walletsManager_.get(), &bs::sync::WalletsManager::blockchainEvent, this, &AuthAddressManager::tryVerifyWalletAddresses);
   connect(walletsManager_.get(), &bs::sync::WalletsManager::authWalletChanged, this, &AuthAddressManager::onAuthWalletChanged);
   connect(walletsManager_.get(), &bs::sync::WalletsManager::walletChanged, this, &AuthAddressManager::onWalletChanged);
   connect(walletsManager_.get(), &bs::sync::WalletsManager::AuthLeafCreated, this, &AuthAddressManager::onWalletCreated);
   connect(walletsManager_.get(), &bs::sync::WalletsManager::AuthLeafNotCreated, this, &AuthAddressManager::ConnectionComplete);

   // signingContainer_ might be null if user rejects remote signer key
   if (signingContainer_) {
      connect(signingContainer_.get(), &SignContainer::TXSigned, this, &AuthAddressManager::onTXSigned);
   }

   SetAuthWallet();

   ArmoryCallbackTarget::init(armory_.get());
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
      logger_->debug("[AuthAddressManager::setup] Auth wallet missing");
      addressVerificator_.reset();
      return false;
   }
   if (addressVerificator_) {
      return true;
   }

   if (readyError() != ReadyError::NoError) {
      return false;
   }

   addressVerificator_ = std::make_shared<AddressVerificator>(logger_, armory_
      , [this](const bs::Address &address, AddressVerificationState state)
   {
      if (!addressVerificator_) {
         logger_->error("[AuthAddressManager::setup] Failed to create AddressVerificator object");
         return;
      }
      if (GetState(address) != state) {
         logger_->info("Address verification {} for {}", to_string(state), address.display());
         SetState(address, state);
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
   tryVerifyWalletAddresses();
   emit AuthWalletChanged();
}

AuthAddressManager::~AuthAddressManager() noexcept
{
   addressVerificator_.reset();
   ArmoryCallbackTarget::cleanup();
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

AuthAddressManager::ReadyError AuthAddressManager::readyError() const
{
   if (!HasAuthAddr()) {
      return ReadyError::MissingAuthAddr;
   }
   if (!HaveBSAddressList()) {
      return ReadyError::MissingAddressList;
   }
   if (!armory_) {
      return ReadyError::MissingArmoryPtr;
   }
   if (!armory_->isOnline()) {
      return ReadyError::ArmoryOffline;
   }

   return ReadyError::NoError;
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
   if (!hasSettlementLeaf(address)) {
      logger_->error("[{}] can't submit without existing settlement leaf", __func__);
      return false;
   }
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
   const auto &itRevoke = signIdsRevoke_.find(id);
   if (itRevoke == signIdsRevoke_.end()) {
      return;
   }
   signIdsRevoke_.erase(id);

   if (result == bs::error::ErrorCode::NoError) {
      if (BroadcastTransaction(signedTX)) {
         emit AuthRevokeTxSent();
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

   if (!addressVerificator_) {
      SPDLOG_LOGGER_ERROR(logger_, "addressVerificator_ is null");
      emit Error(tr("Missing address verificator"));
      return false;
   }

   const auto revokeData = addressVerificator_->getRevokeData(address);
   if (revokeData.first.isNull() || !revokeData.second.isInitialized()) {
      logger_->error("[AuthAddressManager::RevokeAddress] failed to obtain revocation data");
      emit Error(tr("Missing revocation input"));
      return false;
   }

   const auto reqId = signingContainer_->signAuthRevocation(authWallet_->walletId(), address
      , revokeData.second, revokeData.first);
   if (!reqId) {
      logger_->error("[AuthAddressManager::RevokeAddress] failed to send revocation data");
      emit Error(tr("Failed to send revoke"));
      return false;
   }
   signIdsRevoke_.insert(reqId);
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

bool AuthAddressManager::SubmitAddressToPublicBridge(const bs::Address &address)
{
   SubmitAuthAddressForVerificationRequest addressRequest;

   addressRequest.set_username(celerClient_->email());
   addressRequest.set_networktype((settings_->get<NetworkType>(ApplicationSettings::netType) != NetworkType::MainNet)
      ? AddressNetworkType::TestNetType : AddressNetworkType::MainNetType);

   addressRequest.set_address(address.display());

   RequestPacket  request;
   request.set_requesttype(SubmitAuthAddressForVerificationType);
   request.set_requestdata(addressRequest.SerializeAsString());

   logger_->debug("[AuthAddressManager::SubmitAddressToPublicBridge] submitting address {}"
      , address.display());

   return SubmitRequestToPB("submit_address", request.SerializeAsString());
}

void AuthAddressManager::ConfirmSubmitForVerification(BsClient *bsClient, const bs::Address &address)
{
   ConfirmAuthSubmitRequest request;

   request.set_username(celerClient_->email());
   request.set_address(address.display());
   request.set_networktype((settings_->get<NetworkType>(ApplicationSettings::netType) != NetworkType::MainNet)
      ? AddressNetworkType::TestNetType : AddressNetworkType::MainNetType);
   request.set_userid(celerClient_->userId());

   std::string requestData = request.SerializeAsString();
   BinaryData requestDataHash = BtcUtils::getSha256(requestData);

   QPointer<AuthAddressManager> thisPtr = this;

   BsClient::SignAddressReq req;
   req.type = BsClient::SignAddressReq::AuthAddr;
   req.address = address;
   req.invisibleData = requestDataHash;

   req.signedCb = [thisPtr, requestData](const AutheIDClient::SignResult &result) {
      if (!thisPtr) {
         return;
      }

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

      SPDLOG_LOGGER_DEBUG(thisPtr->logger_, "confirmed auth address submission");
      thisPtr->SubmitRequestToPB("confirm_submit_auth_addr", packet.SerializeAsString());
   };

   req.failedCb = [thisPtr](AutheIDClient::ErrorType error) {
      if (!thisPtr) {
         return;
      }

      SPDLOG_LOGGER_ERROR(thisPtr->logger_, "failed to sign data: {}", AutheIDClient::errorString(error).toStdString());
      emit thisPtr->signFailed(error);
   };

   bsClient->signAddress(req);
}

bool AuthAddressManager::CancelSubmitForVerification(BsClient *bsClient, const bs::Address &address)
{
   CancelAuthAddressSubmitRequest request;

   request.set_username(celerClient_->email());
   request.set_address(address.display());
   request.set_userid(celerClient_->userId());

   RequestPacket  packet;

   packet.set_requesttype(CancelAuthAddressSubmitType);
   packet.set_requestdata(request.SerializeAsString());

   logger_->debug("[AuthAddressManager::CancelSubmitForVerification] cancel submission of {}"
      , address.display());

   if (bsClient) {
      bsClient->cancelSign();
   }

   return SubmitRequestToPB("confirm_submit_auth_addr", packet.SerializeAsString());
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

   auto&& address = bs::Address::fromAddressString(response.address());
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
            , address.display(), response.errormessage());
         emit Error(tr("Authentication Address rejected: %1").arg(QString::fromStdString(response.errormessage())));
      } else {
         logger_->error("[AuthAddressManager::ProcessSubmitAuthAddressResponse] auth address {} rejected"
            , address.display());
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

   auto&& address = bs::Address::fromAddressString(response.address());
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

   auto&& address = bs::Address::fromAddressString(response.address());
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
   emit Error(tr("Received error from BS server: %1").arg(QString::fromStdString(response.errormessage())));
}

void AuthAddressManager::tryVerifyWalletAddresses()
{
   std::string errorMsg;
   ReadyError state = readyError();
   if (state != ReadyError::NoError) {
      SPDLOG_LOGGER_DEBUG(logger_, "can't start auth address verification: {}", readyErrorStr(state));
      return;
   }

   setup();

   VerifyWalletAddressesFunction();
}

void AuthAddressManager::VerifyWalletAddressesFunction()
{
   logger_->debug("[AuthAddressManager::VerifyWalletAddressesFunction] Starting to VerifyWalletAddresses");

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
         defaultAddr_ = bs::Address::fromAddressString(defaultAuthAddrStr.toStdString());
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
      addressVerificator_->addAddress(addr);
   }
   addressVerificator_->startAddressVerification();

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

      for (size_t i = addresses_.size(); i < count; i++) {
         const auto &addr = newAddresses[i];
         AddAddress(addr);
         if (addressVerificator_) {
            addressVerificator_->addAddress(addr);
         }
      }
   }

   if (listUpdated && addressVerificator_) {
      addressVerificator_->startAddressVerification();
      emit AddressListUpdated();
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

std::string AuthAddressManager::readyErrorStr(AuthAddressManager::ReadyError error)
{
   switch (error) {
      case ReadyError::NoError:              return "NoError";
      case ReadyError::MissingAuthAddr:      return "MissingAuthAddr";
      case ReadyError::MissingAddressList:   return "MissingAddressList";
      case ReadyError::MissingArmoryPtr:     return "MissingArmoryPtr";
      case ReadyError::ArmoryOffline:        return "ArmoryOffline";
   }
   return "Unknown";
}

bool AuthAddressManager::SendGetBSAddressListRequest()
{
   GetBSFundingAddressListRequest addressRequest;
   RequestPacket  request;

   addressRequest.set_addresslisttype(BitcoinsAddressType);

   request.set_requesttype(GetBSFundingAddressListType);
   request.set_requestdata(addressRequest.SerializeAsString());

   return SubmitRequestToPB("get_bs_list", request.SerializeAsString());
}

bool AuthAddressManager::SubmitRequestToPB(const std::string& name, const std::string& data)
{
   QMetaObject::invokeMethod(this, [this, name, data] {
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
         return;
      }

      activeCommands_.emplace(requestId, std::move(command));
   });

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
   tryVerifyWalletAddresses();
}

AddressVerificationState AuthAddressManager::GetState(const bs::Address &addr) const
{
   FastLock lock(statesLock_);
   const auto itState = states_.find(addr);
   if (itState == states_.end()) {
      return AddressVerificationState::InProgress;
   }
   return itState->second;
}

void AuthAddressManager::SetState(const bs::Address &addr, AddressVerificationState state)
{
   const auto prevState = GetState(addr);
   if ((prevState == AddressVerificationState::Submitted) && (state == AddressVerificationState::NotSubmitted)) {
      return;
   }

   {
      FastLock lock(statesLock_);
      states_[addr] = state;
   }

   if ((state == AddressVerificationState::Verified) && (prevState == AddressVerificationState::PendingVerification)) {
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

bool AuthAddressManager::IsAtLeastOneAwaitingVerification() const
{
   {
      FastLock locker(lockList_);
      for (const auto &address : addresses_) {
         auto addrState = GetState(address);
         if (addrState == AddressVerificationState::Submitted
            || addrState == AddressVerificationState::PendingVerification
            || addrState == AddressVerificationState::VerificationSubmitted) {
            return true;
         }
      }
   }
   return false;
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

void AuthAddressManager::onStateChanged(ArmoryState)
{
   QMetaObject::invokeMethod(this, [this]{
      tryVerifyWalletAddresses();
   });
}

template <typename TVal> TVal AuthAddressManager::lookup(const bs::Address &key, const std::map<bs::Address, TVal> &container) const
{
   const auto it = container.find(key);
   if (it == container.end()) {
      return TVal();
   }
   return it->second;
}

void AuthAddressManager::createAuthWallet(const std::function<void()> &cb)
{
   if (!signingContainer_ || !walletsManager_) {
      emit Error(tr("Unable to create auth wallet"));
      return;
   }

   if (walletsManager_->getAuthWallet() != nullptr) {
      emit Error(tr("Authentication wallet already exists"));
      return;
   }

   if (!walletsManager_->createAuthLeaf(cb)) {
      emit Error(tr("Failed to initate auth wallet creation"));
      return;
   }
}

void AuthAddressManager::onWalletCreated()
{
   emit ConnectionComplete();

   auto authLeaf = walletsManager_->getAuthWallet();

   if (authLeaf != nullptr) {
      emit AuthWalletCreated(QString::fromStdString(authLeaf->walletId()));
   } else {
      logger_->error("[AuthAddressManager::onWalletCreated] we should be able to get auth wallet at this point");
   }
}

std::shared_ptr<bs::sync::hd::SettlementLeaf> AuthAddressManager::getSettlementLeaf(const bs::Address &addr) const
{
   const auto priWallet = walletsManager_->getPrimaryWallet();
   if (!priWallet) {
      logger_->warn("[{}] no primary wallet", __func__);
      return nullptr;
   }
   const auto group = priWallet->getGroup(bs::hd::BlockSettle_Settlement);
   std::shared_ptr<bs::sync::hd::SettlementLeaf> settlLeaf;
   if (group) {
      const auto settlGroup = std::dynamic_pointer_cast<bs::sync::hd::SettlementGroup>(group);
      if (!settlGroup) {
         logger_->error("[{}] wrong settlement group type", __func__);
         return nullptr;
      }
      settlLeaf = settlGroup->getLeaf(addr);
   }
   return settlLeaf;
}

void AuthAddressManager::createSettlementLeaf(const bs::Address &addr
   , const std::function<void()> &cb)
{
   const auto &cbPubKey = [this, cb](const SecureBinaryData &pubKey) {
      if (pubKey.isNull()) {
         return;
      }
      if (cb) {
         cb();
      }
   };
   walletsManager_->createSettlementLeaf(addr, cbPubKey);
}
