#include "HeadlessContainer.h"

#include "ApplicationSettings.h"
#include "ConnectionManager.h"
#include "Wallets/SyncSettlementWallet.h"
#include "Wallets/SyncHDWallet.h"
#include "Wallets/SyncWalletsManager.h"
#include "SystemFileUtils.h"

#include <QCoreApplication>
#include <QDataStream>
#include <QDebug>
#include <QDir>
#include <QProcess>
#include <QStandardPaths>
#include <QtConcurrent/QtConcurrentRun>

#include <spdlog/spdlog.h>
#include "signer.pb.h"

namespace {

   constexpr int kKillTimeout = 5000;
   constexpr int kStartTimeout = 5000;

   // When remote signer will try to reconnect
   constexpr auto kRemoteReconnectPeriod = std::chrono::seconds(10);

} // namespace

#ifdef Q_OS_WIN
#include <windows.h>
#include <tlhelp32.h>
#define TERM_SUCCESS_CLEAN 0
#define TERM_SUCCESS_KILL 1
#define TERM_FAILED 2

#ifndef MAKEULONGLONG
#define MAKEULONGLONG(ldw, hdw) ((ULONGLONG(hdw) << 32) | ((ldw) & 0xFFFFFFFF))
#endif

#ifndef MAXULONGLONG
#define MAXULONGLONG ((ULONGLONG)~((ULONGLONG)0))
#endif

static DWORD WINAPI GetWinMainThreadId(DWORD dwProcID)
{
   DWORD dwMainThreadID = 0;
   ULONGLONG ullMinCreateTime = MAXULONGLONG;

   HANDLE hThreadSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
   if (hThreadSnap != INVALID_HANDLE_VALUE) {
      THREADENTRY32 th32;
      th32.dwSize = sizeof(THREADENTRY32);
      BOOL bOK = TRUE;
      for (bOK = Thread32First(hThreadSnap, &th32); bOK;
           bOK = Thread32Next(hThreadSnap, &th32)) {
         if (th32.th32OwnerProcessID == dwProcID) {
            HANDLE hThread = OpenThread(THREAD_QUERY_INFORMATION, TRUE, th32.th32ThreadID);
            if (hThread) {
               FILETIME afTimes[4] = {0};
               if (GetThreadTimes(hThread,
                                  &afTimes[0], &afTimes[1], &afTimes[2], &afTimes[3])) {
                  ULONGLONG ullTest = MAKEULONGLONG(afTimes[0].dwLowDateTime,
                        afTimes[0].dwHighDateTime);
                  if (ullTest && ullTest < ullMinCreateTime) {
                     ullMinCreateTime = ullTest;
                     dwMainThreadID = th32.th32ThreadID; // let it be main... :)
                  }
               }
               CloseHandle(hThread);
            }
         }
      }
      CloseHandle(hThreadSnap);
   }

   return dwMainThreadID;
}

static DWORD WINAPI TerminateWinApp(DWORD dwPID, DWORD dwTimeout)
{
   HANDLE   hProc ;
   DWORD   dwRet ;

   hProc = OpenProcess(SYNCHRONIZE|PROCESS_TERMINATE, FALSE, dwPID);
   if (hProc == NULL) {
      return TERM_FAILED ;
   }

   PostThreadMessage(GetWinMainThreadId(dwPID), WM_QUIT, 0, 0);


   // Wait on the handle. If it signals, great. If it times out,
   // then you kill it.
   if(WaitForSingleObject(hProc, dwTimeout) != WAIT_OBJECT_0) {
      dwRet = (TerminateProcess(hProc, 0) ? TERM_SUCCESS_KILL : TERM_FAILED);
   }
   else {
      dwRet = TERM_SUCCESS_CLEAN ;
   }

   CloseHandle(hProc) ;

   return dwRet;
}

#endif // Q_OS_WIN

using namespace Blocksettle::Communication;
Q_DECLARE_METATYPE(headless::RequestPacket)
Q_DECLARE_METATYPE(std::shared_ptr<bs::sync::hd::Leaf>)

NetworkType HeadlessContainer::mapNetworkType(headless::NetworkType netType)
{
   switch (netType) {
   case headless::MainNetType:   return NetworkType::MainNet;
   case headless::TestNetType:   return NetworkType::TestNet;
   default:                      return NetworkType::Invalid;
   }
}

void HeadlessContainer::makeCreateHDWalletRequest(const std::string &name, const std::string &desc, bool primary
   , const bs::core::wallet::Seed &seed, const std::vector<bs::wallet::PasswordData> &pwdData, bs::wallet::KeyRank keyRank
   , headless::CreateHDWalletRequest &request)
{
   if (!pwdData.empty()) {
      request.set_rankm(keyRank.first);
      request.set_rankn(keyRank.second);
   }
   for (const auto &pwd : pwdData) {
      auto reqPwd = request.add_password();
      reqPwd->set_password(pwd.password.toHexStr());
      reqPwd->set_enctype(static_cast<uint32_t>(pwd.encType));
      reqPwd->set_enckey(pwd.encKey.toBinStr());
   }
   auto wallet = request.mutable_wallet();
   wallet->set_name(name);
   wallet->set_description(desc);
   wallet->set_nettype((seed.networkType() == NetworkType::TestNet) ? headless::TestNetType : headless::MainNetType);
   if (primary) {
      wallet->set_primary(true);
   }
   if (!seed.empty()) {
      if (seed.hasPrivateKey()) {
         wallet->set_privatekey(seed.privateKey().toBinStr());
         wallet->set_chaincode(seed.chainCode().toBinStr());
      }
      else if (!seed.seed().isNull()) {
         wallet->set_seed(seed.seed().toBinStr());
      }
   }
}

void HeadlessListener::OnDataReceived(const std::string& data)
{
   headless::RequestPacket packet;
   if (!packet.ParseFromString(data)) {
      logger_->error("[HeadlessListener] failed to parse request packet");
      return;
   }
   if (packet.id() > id_) {
      logger_->error("[HeadlessListener] reply id inconsistency: {} > {}", packet.id(), id_);
      emit error(HeadlessContainer::InvalidProtocol, tr("reply id inconsistency"));
      return;
   }

   if (packet.type() == headless::DisconnectionRequestType) {
      OnDisconnected();
      return;
   }
   else if (packet.type() == headless::AuthenticationRequestType) {
      headless::AuthenticationReply response;
      if (!response.ParseFromString(packet.data())) {
         logger_->error("[HeadlessListener] failed to parse auth reply");
         emit error(HeadlessContainer::SerializationFailed, tr("failed to parse auth reply"));
         return;
      }

      if (HeadlessContainer::mapNetworkType(response.nettype()) != netType_) {
         logger_->error("[HeadlessListener] network type mismatch");
         emit error(HeadlessContainer::NetworkTypeMismatch, tr("network type mismatch"));
         return;
      }

      // BIP 150/151 should be be complete by this point.
      hasUI_ = response.hasui();
      isReady_ = true;
      emit authenticated();
   }
   else {
      emit PacketReceived(packet);
   }
}

void HeadlessListener::OnConnected()
{
   logger_->debug("[HeadlessListener] Connected");
   emit connected();
}

void HeadlessListener::OnDisconnected()
{
   logger_->debug("[HeadlessListener] Disconnected");
   isReady_ = false;
   emit disconnected();
}

void HeadlessListener::OnError(DataConnectionListener::DataConnectionError errorCode)
{
   logger_->debug("[HeadlessListener] error {}", errorCode);
   isReady_ = false;

   switch (errorCode) {
      case UndefinedSocketError:
         emit error(HeadlessContainer::SocketFailed, tr("socket error"));
         break;
      case HostNotFoundError:
         emit error(HeadlessContainer::HostNotFound, tr("host not found"));
         break;
      case HandshakeFailed:
         emit error(HeadlessContainer::HandshakeFailed, tr("encryption handshake failed"));
         break;
      case SerializationFailed:
         emit error(HeadlessContainer::SerializationFailed, tr("serialization failed"));
         break;
      case HeartbeatWaitFailed:
         emit error(HeadlessContainer::HeartbeatWaitFailed, tr("Connection lost"));
         break;
      default:
         emit error(HeadlessContainer::UnknownError, tr("unknown error"));
         break;
   }

   // Need to disconnect connection because otherwise it will continue send error responses over and over
   connection_->closeConnection();
}

bs::signer::RequestId HeadlessListener::Send(headless::RequestPacket packet, bool updateId)
{
   bs::signer::RequestId id = 0;
   if (updateId) {
      id = newRequestId();
      packet.set_id(id);
   }
   if (!connection_->send(packet.SerializeAsString())) {
      logger_->error("[HeadlessListener] Failed to send request packet");
      emit disconnected();
      return 0;
   }
   return id;
}

HeadlessContainer::HeadlessContainer(const std::shared_ptr<spdlog::logger> &logger, OpMode opMode)
   : SignContainer(logger, opMode)
{
   qRegisterMetaType<headless::RequestPacket>();
   qRegisterMetaType<std::shared_ptr<bs::sync::hd::Leaf>>();
}

bs::signer::RequestId HeadlessContainer::Send(headless::RequestPacket packet, bool incSeqNo)
{
   if (!listener_) {
      return 0;
   }
   return listener_->Send(packet, incSeqNo);
}

void HeadlessContainer::ProcessSignTXResponse(unsigned int id, const std::string &data)
{
   headless::SignTXReply response;
   if (!response.ParseFromString(data)) {
      logger_->error("[HeadlessContainer] Failed to parse SignTXReply");
      emit TXSigned(id, {}, "failed to parse", false);
      return;
   }
   emit TXSigned(id, response.signedtx(), response.error(), response.cancelledbyuser());
}

void HeadlessContainer::ProcessPasswordRequest(const std::string &data)
{
   headless::PasswordRequest request;
   if (!request.ParseFromString(data)) {
      logger_->error("[HeadlessContainer] Failed to parse PasswordRequest");
      return;
   }
   emit PasswordRequested(bs::hd::WalletInfo(request), request.prompt());
}

void HeadlessContainer::ProcessCreateHDWalletResponse(unsigned int id, const std::string &data)
{
   headless::CreateHDWalletResponse response;
   if (!response.ParseFromString(data)) {
      logger_->error("[HeadlessContainer] Failed to parse CreateHDWallet reply");
      emit Error(id, "failed to parse");
      return;
   }
   if (response.has_leaf()) {
      const auto path = bs::hd::Path::fromString(response.leaf().path());
      bs::core::wallet::Type leafType = bs::core::wallet::Type::Unknown;
      switch (static_cast<bs::hd::CoinType>(path.get(-2))) {
      case bs::hd::CoinType::Bitcoin_main:
      case bs::hd::CoinType::Bitcoin_test:
         leafType = bs::core::wallet::Type::Bitcoin;
         break;
      case bs::hd::CoinType::BlockSettle_Auth:
         leafType = bs::core::wallet::Type::Authentication;
         break;
      case bs::hd::CoinType::BlockSettle_CC:
         leafType = bs::core::wallet::Type::ColorCoin;
         break;
      default:    break;
      }
      const auto leaf = std::make_shared<bs::sync::hd::Leaf>(response.leaf().walletid()
         , response.leaf().name(), response.leaf().desc(), this, logger_
         , leafType, response.leaf().extonly());
      logger_->debug("[HeadlessContainer] HDLeaf {} created", response.leaf().walletid());
      emit HDLeafCreated(id, leaf);
   }
   else if (response.has_wallet()) {
      const auto netType = (response.wallet().nettype() == headless::TestNetType) ? NetworkType::TestNet : NetworkType::MainNet;
      auto wallet = std::make_shared<bs::sync::hd::Wallet>(netType, response.wallet().walletid()
         , response.wallet().name(), response.wallet().description(), this, logger_);

      for (int i = 0; i < response.wallet().groups_size(); i++) {
         const auto grpPath = bs::hd::Path::fromString(response.wallet().groups(i).path());
         if (grpPath.length() != 2) {
            logger_->warn("[HeadlessContainer] invalid path[{}]: {}", i, response.wallet().groups(i).path());
            continue;
         }
         const auto grpType = static_cast<bs::hd::CoinType>(grpPath.get((int)grpPath.length() - 1));
         auto group = wallet->createGroup(grpType);

         for (int j = 0; j < response.wallet().leaves_size(); j++) {
            const auto responseLeaf = response.wallet().leaves(j);
            const auto leafPath = bs::hd::Path::fromString(responseLeaf.path());
            if (leafPath.length() != 3) {
               logger_->warn("[HeadlessContainer] invalid path[{}]: {}", j, response.wallet().leaves(j).path());
               continue;
            }
            if (leafPath.get((int)leafPath.length() - 2) != static_cast<bs::hd::Path::Elem>(grpType)) {
               continue;
            }
            group->createLeaf(leafPath.get(-1), responseLeaf.walletid());
         }
         wallet->synchronize([] {});
      }
      logger_->debug("[HeadlessContainer] HDWallet {} created", wallet->walletId());
      emit HDWalletCreated(id, wallet);
   }
   else {
      emit Error(id, response.error());
   }
}

void HeadlessContainer::ProcessGetRootKeyResponse(unsigned int id, const std::string &data)
{
   headless::GetRootKeyResponse response;
   if (!response.ParseFromString(data)) {
      logger_->error("[HeadlessContainer] Failed to parse GetRootKey reply");
      emit Error(id, "failed to parse");
      return;
   }
   if (response.decryptedprivkey().empty()) {
      emit Error(id, response.walletid());
   }
   else {
      emit DecryptedRootKey(id, response.decryptedprivkey(), response.chaincode(), response.walletid());
   }
}

void HeadlessContainer::ProcessGetHDWalletInfoResponse(unsigned int id, const std::string &data)
{
   headless::GetHDWalletInfoResponse response;
   if (!response.ParseFromString(data)) {
      logger_->error("[HeadlessContainer] Failed to parse GetHDWalletInfo reply");
      emit Error(id, "failed to parse");
      return;
   }
   if (response.error().empty()) {
      emit QWalletInfo(id, bs::hd::WalletInfo(response));
   }
   else {
      missingWallets_.insert(response.rootwalletid());
      emit Error(id, response.error());
   }
}

void HeadlessContainer::ProcessSetLimitsResponse(unsigned int id, const std::string &data)
{
   headless::SetLimitsResponse response;
   if (!response.ParseFromString(data)) {
      logger_->error("[HeadlessContainer] Failed to parse SetLimits reply");
      emit Error(id, "failed to parse");
      return;
   }
   emit AutoSignStateChanged(response.rootwalletid(), response.autosignactive(), response.error());
}

bs::signer::RequestId HeadlessContainer::signTXRequest(const bs::core::wallet::TXSignRequest &txSignReq
   , bool autoSign, SignContainer::TXSignMode mode, const PasswordType& password
   , bool keepDuplicatedRecipients)
{
   if (!txSignReq.isValid()) {
      logger_->error("[HeadlessContainer] Invalid TXSignRequest");
      return 0;
   }
   headless::SignTXRequest request;
   request.set_walletid(txSignReq.walletId);
   request.set_keepduplicatedrecipients(keepDuplicatedRecipients);
   if (autoSign) {
      request.set_applyautosignrules(true);
   }
   if (txSignReq.populateUTXOs) {
      request.set_populateutxos(true);
   }

   for (const auto &utxo : txSignReq.inputs) {
      request.add_inputs(utxo.serialize().toBinStr());
   }

   for (const auto &recip : txSignReq.recipients) {
      request.add_recipients(recip->getSerializedScript().toBinStr());
   }
   if (txSignReq.fee) {
      request.set_fee(txSignReq.fee);
   }

   if (txSignReq.RBF) {
      request.set_rbf(true);
   }

   if (!password.isNull()) {
      request.set_password(password.toHexStr());
   }

   if (!txSignReq.prevStates.empty()) {
      request.set_unsignedstate(txSignReq.serializeState().toBinStr());
   }

   if (txSignReq.change.value) {
      auto change = request.mutable_change();
      change->set_address(txSignReq.change.address.display());
      change->set_index(txSignReq.change.index);
      change->set_value(txSignReq.change.value);
   }

   headless::RequestPacket packet;
   switch (mode) {
   case TXSignMode::Full:
      packet.set_type(headless::SignTXRequestType);
      break;

   case TXSignMode::Partial:
      packet.set_type(headless::SignPartialTXRequestType);
      break;

   default:    break;
   }
   packet.set_data(request.SerializeAsString());
   const auto id = Send(packet);
   signRequests_.insert(id);
   return id;
}

unsigned int HeadlessContainer::signPartialTXRequest(const bs::core::wallet::TXSignRequest &req
   , bool autoSign, const PasswordType& password)
{
   return signTXRequest(req, autoSign, TXSignMode::Partial, password);
}

bs::signer::RequestId HeadlessContainer::signPayoutTXRequest(const bs::core::wallet::TXSignRequest &txSignReq
   , const bs::Address &authAddr, const std::string &settlementId
   , bool autoSign, const PasswordType& password)
{
   if ((txSignReq.inputs.size() != 1) || (txSignReq.recipients.size() != 1) || settlementId.empty()) {
      logger_->error("[HeadlessContainer] Invalid PayoutTXSignRequest");
      return 0;
   }
   headless::SignPayoutTXRequest request;
   request.set_input(txSignReq.inputs[0].serialize().toBinStr());
   request.set_recipient(txSignReq.recipients[0]->getSerializedScript().toBinStr());
   request.set_authaddress(authAddr.display());
   request.set_settlementid(settlementId);
   if (autoSign) {
      request.set_applyautosignrules(autoSign);
   }

   if (!password.isNull()) {
      request.set_password(password.toHexStr());
   }

   headless::RequestPacket packet;
   packet.set_type(headless::SignPayoutTXRequestType);
   packet.set_data(request.SerializeAsString());
   const auto id = Send(packet);
   signRequests_.insert(id);
   return id;
}

bs::signer::RequestId HeadlessContainer::signMultiTXRequest(const bs::core::wallet::TXMultiSignRequest &txMultiReq)
{
   if (!txMultiReq.isValid()) {
      logger_->error("[HeadlessContainer] Invalid TXMultiSignRequest");
      return 0;
   }

   Signer signer;
   signer.setFlags(SCRIPT_VERIFY_SEGWIT);

   headless::SignTXMultiRequest request;
   for (const auto &input : txMultiReq.inputs) {
      request.add_walletids(input.second);
      signer.addSpender(std::make_shared<ScriptSpender>(input.first));
   }
   for (const auto &recip : txMultiReq.recipients) {
      signer.addRecipient(recip);
   }
   request.set_signerstate(signer.serializeState().toBinStr());

   headless::RequestPacket packet;
   packet.set_type(headless::SignTXMultiRequestType);
   packet.set_data(request.SerializeAsString());
   const auto id = Send(packet);
   signRequests_.insert(id);
   return id;
}

bs::signer::RequestId HeadlessContainer::CancelSignTx(const BinaryData &txId)
{
   headless::CancelSignTx request;
   request.set_txid(txId.toBinStr());

   headless::RequestPacket packet;
   packet.set_type(headless::CancelSignTxRequestType);
   packet.set_data(request.SerializeAsString());
   return Send(packet);
}

void HeadlessContainer::SendPassword(const std::string &walletId, const PasswordType &password,
   bool cancelledByUser)
{
   headless::RequestPacket packet;
   packet.set_type(headless::PasswordRequestType);

   headless::PasswordReply response;
   if (!walletId.empty()) {
      response.set_walletid(walletId);
   }
   if (!password.isNull()) {
      response.set_password(password.toHexStr());
   }
   response.set_cancelledbyuser(cancelledByUser);
   packet.set_data(response.SerializeAsString());
   Send(packet, false);
}

bs::signer::RequestId HeadlessContainer::SetUserId(const BinaryData &userId)
{
   if (!listener_) {
      logger_->warn("[HeadlessContainer::SetUserId] listener not set yet");
      return 0;
   }

   headless::SetUserIdRequest request;
   if (!userId.isNull()) {
      request.set_userid(userId.toBinStr());
   }

   headless::RequestPacket packet;
   packet.set_type(headless::SetUserIdRequestType);
   packet.set_data(request.SerializeAsString());
   return Send(packet);
}

bs::signer::RequestId HeadlessContainer::createHDLeaf(const std::string &rootWalletId
   , const bs::hd::Path &path, const std::vector<bs::wallet::PasswordData> &pwdData)
{
   if (rootWalletId.empty() || (path.length() != 3)) {
      logger_->error("[HeadlessContainer] Invalid input data for HD wallet creation");
      return 0;
   }
   headless::CreateHDWalletRequest request;
   auto leaf = request.mutable_leaf();
   leaf->set_rootwalletid(rootWalletId);
   leaf->set_path(path.toString());
   for (const auto &pwd : pwdData) {
      auto reqPwd = request.add_password();
      reqPwd->set_password(pwd.password.toHexStr());
      reqPwd->set_enctype(static_cast<uint32_t>(pwd.encType));
      reqPwd->set_enckey(pwd.encKey.toBinStr());
   }

   headless::RequestPacket packet;
   packet.set_type(headless::CreateHDWalletRequestType);
   packet.set_data(request.SerializeAsString());
   return Send(packet);
}

bs::signer::RequestId HeadlessContainer::createHDWallet(const std::string &name
   , const std::string &desc, bool primary, const bs::core::wallet::Seed &seed
   , const std::vector<bs::wallet::PasswordData> &pwdData, bs::wallet::KeyRank keyRank)
{
   headless::CreateHDWalletRequest request;
   makeCreateHDWalletRequest(name, desc, primary, seed, pwdData, keyRank, request);

   headless::RequestPacket packet;
   packet.set_type(headless::CreateHDWalletRequestType);
   packet.set_data(request.SerializeAsString());
   return Send(packet);
}

bs::signer::RequestId HeadlessContainer::DeleteHDRoot(const std::string &rootWalletId)
{
   if (rootWalletId.empty()) {
      return 0;
   }
   return SendDeleteHDRequest(rootWalletId, {});
}

bs::signer::RequestId HeadlessContainer::DeleteHDLeaf(const std::string &leafWalletId)
{
   if (leafWalletId.empty()) {
      return 0;
   }
   return SendDeleteHDRequest({}, leafWalletId);
}

bs::signer::RequestId HeadlessContainer::SendDeleteHDRequest(const std::string &rootWalletId, const std::string &leafId)
{
   headless::DeleteHDWalletRequest request;
   if (!rootWalletId.empty()) {
      request.set_rootwalletid(rootWalletId);
   }
   else if (!leafId.empty()) {
      request.set_leafwalletid(leafId);
   }
   else {
      logger_->error("[HeadlessContainer] can't send delete request - both IDs are empty");
      return 0;
   }

   headless::RequestPacket packet;
   packet.set_type(headless::DeleteHDWalletRequestType);
   packet.set_data(request.SerializeAsString());
   return Send(packet);
}

void HeadlessContainer::setLimits(const std::string &walletId, const SecureBinaryData &pass
   , bool autoSign)
{
   if (walletId.empty()) {
      logger_->error("[HeadlessContainer] no walletId for SetLimits");
      return;
   }
   headless::SetLimitsRequest request;
   request.set_rootwalletid(walletId);
   if (!pass.isNull()) {
      request.set_password(pass.toHexStr());
   }
   request.set_activateautosign(autoSign);

   headless::RequestPacket packet;
   packet.set_type(headless::SetLimitsRequestType);
   packet.set_data(request.SerializeAsString());
   Send(packet);
}

bs::signer::RequestId HeadlessContainer::customDialogRequest(bs::signer::ui::DialogType signerDialog, const QVariantMap &data)
{
   // serialize variant data
   QByteArray ba;
   QDataStream stream(&ba, QIODevice::WriteOnly);
   stream << data;

   headless::CustomDialogRequest request;
   request.set_dialogname(bs::signer::ui::getSignerDialogPath(signerDialog).toStdString());
   request.set_variantdata(ba.data(), ba.size());

   headless::RequestPacket packet;
   packet.set_type(headless::ExecCustomDialogRequestType);
   packet.set_data(request.SerializeAsString());
   return Send(packet);
}

bs::signer::RequestId HeadlessContainer::getDecryptedRootKey(const std::string &walletId
   , const SecureBinaryData &password)
{
   headless::GetRootKeyRequest request;
   request.set_rootwalletid(walletId);
   if (!password.isNull()) {
      request.set_password(password.toHexStr());
   }

   headless::RequestPacket packet;
   packet.set_type(headless::GetRootKeyRequestType);
   packet.set_data(request.SerializeAsString());
   return Send(packet);
}

bs::signer::RequestId HeadlessContainer::GetInfo(const std::string &rootWalletId)
{
   if (rootWalletId.empty()) {
      return 0;
   }
   headless::GetHDWalletInfoRequest request;
   request.set_rootwalletid(rootWalletId);

   headless::RequestPacket packet;
   packet.set_type(headless::GetHDWalletInfoRequestType);
   packet.set_data(request.SerializeAsString());
   return Send(packet);
}

bool HeadlessContainer::isReady() const
{
   return (listener_ != nullptr) && listener_->isReady();
}

bool HeadlessContainer::isWalletOffline(const std::string &walletId) const
{
   return ((missingWallets_.find(walletId) != missingWallets_.end())
      || (woWallets_.find(walletId) != woWallets_.end()));
}

void HeadlessContainer::createSettlementWallet(const std::function<void(const std::shared_ptr<bs::sync::SettlementWallet> &)> &cb)
{
   headless::RequestPacket packet;
   packet.set_type(headless::CreateSettlWalletType);
   const auto reqId = Send(packet);
   cbSettlWalletMap_[reqId] = cb;
}

void HeadlessContainer::syncWalletInfo(const std::function<void(std::vector<bs::sync::WalletInfo>)> &cb)
{
   headless::RequestPacket packet;
   packet.set_type(headless::SyncWalletInfoType);
   const auto reqId = Send(packet);
   cbWalletInfoMap_[reqId] = cb;
}

void HeadlessContainer::syncHDWallet(const std::string &id, const std::function<void(bs::sync::HDWalletData)> &cb)
{
   headless::SyncWalletRequest request;
   request.set_walletid(id);

   headless::RequestPacket packet;
   packet.set_type(headless::SyncHDWalletType);
   packet.set_data(request.SerializeAsString());
   const auto reqId = Send(packet);
   cbHDWalletMap_[reqId] = cb;
}

void HeadlessContainer::syncWallet(const std::string &id, const std::function<void(bs::sync::WalletData)> &cb)
{
   headless::SyncWalletRequest request;
   request.set_walletid(id);

   headless::RequestPacket packet;
   packet.set_type(headless::SyncWalletType);
   packet.set_data(request.SerializeAsString());
   const auto reqId = Send(packet);
   cbWalletMap_[reqId] = cb;
}

void HeadlessContainer::syncAddressComment(const std::string &walletId, const bs::Address &addr
   , const std::string &comment)
{
   headless::SyncCommentRequest request;
   request.set_walletid(walletId);
   request.set_address(addr.display());
   request.set_comment(comment);

   headless::RequestPacket packet;
   packet.set_type(headless::SyncCommentType);
   packet.set_data(request.SerializeAsString());
   Send(packet);
}

void HeadlessContainer::syncTxComment(const std::string &walletId, const BinaryData &txHash
   , const std::string &comment)
{
   headless::SyncCommentRequest request;
   request.set_walletid(walletId);
   request.set_txhash(txHash.toBinStr());
   request.set_comment(comment);

   headless::RequestPacket packet;
   packet.set_type(headless::SyncCommentType);
   packet.set_data(request.SerializeAsString());
   Send(packet);
}

static headless::AddressType mapFrom(AddressEntryType aet)
{
   switch (aet) {
   case AddressEntryType_Default:   return headless::AddressType_Default;
   case AddressEntryType_P2PKH:     return headless::AddressType_P2PKH;
   case AddressEntryType_P2PK:      return headless::AddressType_P2PK;
   case AddressEntryType_P2WPKH:    return headless::AddressType_P2WPKH;
   case AddressEntryType_Multisig:  return headless::AddressType_Multisig;
   case AddressEntryType_P2SH:      return headless::AddressType_P2SH;
   case AddressEntryType_P2WSH:     return headless::AddressType_P2WSH;
   default:    return headless::AddressType_Default;
   }
}

void HeadlessContainer::syncNewAddress(const std::string &walletId, const std::string &index
   , AddressEntryType aet, const std::function<void(const bs::Address &)> &cb)
{
   const auto &cbWrap = [cb](const std::vector<std::pair<bs::Address, std::string>> &addrs) {
      if (!addrs.empty()) {
         cb(addrs[0].first);
      }
      else {
         cb({});
      }
   };

   headless::SyncAddressesRequest request;
   request.set_walletid(walletId);
   request.set_persistent(true);
   auto idx = request.add_indices();
   idx->set_index(index);
   idx->set_addrtype(mapFrom(aet));

   headless::RequestPacket packet;
   packet.set_type(headless::SyncAddressesType);
   packet.set_data(request.SerializeAsString());
   const auto reqId = Send(packet);
   cbNewAddrsMap_[reqId] = cbWrap;
}

void HeadlessContainer::syncNewAddresses(const std::string &walletId
   , const std::vector<std::pair<std::string, AddressEntryType>> &indices
   , const std::function<void(const std::vector<std::pair<bs::Address, std::string>> &)> &cb
   , bool persistent)
{
   headless::SyncAddressesRequest request;
   request.set_walletid(walletId);
   for (const auto &index : indices) {
      auto idx = request.add_indices();
      idx->set_index(index.first);
      idx->set_addrtype(mapFrom(index.second));
   }
   request.set_persistent(persistent);

   headless::RequestPacket packet;
   packet.set_type(headless::SyncAddressesType);
   packet.set_data(request.SerializeAsString());
   const auto reqId = Send(packet);
   cbNewAddrsMap_[reqId] = cb;
}

static NetworkType mapFrom(headless::NetworkType netType)
{
   switch (netType) {
   case headless::MainNetType:   return NetworkType::MainNet;
   case headless::TestNetType:   return NetworkType::TestNet;
   default:    return NetworkType::Invalid;
   }
}

static bs::sync::WalletFormat mapFrom(headless::WalletFormat format)
{
   switch (format) {
   case headless::WalletFormatHD:         return bs::sync::WalletFormat::HD;
   case headless::WalletFormatPlain:      return bs::sync::WalletFormat::Plain;
   case headless::WalletFormatSettlement: return bs::sync::WalletFormat::Settlement;
   case headless::WalletFormatUnknown:
   default:    return bs::sync::WalletFormat::Unknown;
   }
}

void HeadlessContainer::ProcessSettlWalletCreate(unsigned int id, const std::string &data)
{
   headless::SettlWalletResponse response;
   if (!response.ParseFromString(data)) {
      logger_->error("[{}] Failed to parse reply", __func__);
      emit Error(id, "failed to parse");
      return;
   }
   const auto itCb = cbSettlWalletMap_.find(id);
   if (itCb == cbSettlWalletMap_.end()) {
      emit Error(id, "no callback found for id " + std::to_string(id));
      return;
   }
   const auto settlWallet = std::make_shared<bs::sync::SettlementWallet>(response.walletid()
      , response.name(), response.description(), this, logger_);
   itCb->second(settlWallet);
}

void HeadlessContainer::ProcessSyncWalletInfo(unsigned int id, const std::string &data)
{
   headless::SyncWalletInfoResponse response;
   if (!response.ParseFromString(data)) {
      logger_->error("[{}] Failed to parse reply", __func__);
      emit Error(id, "failed to parse");
      return;
   }
   const auto itCb = cbWalletInfoMap_.find(id);
   if (itCb == cbWalletInfoMap_.end()) {
      emit Error(id, "no callback found for id " + std::to_string(id));
      return;
   }
   std::vector<bs::sync::WalletInfo> result;
   for (int i = 0; i < response.wallets_size(); ++i) {
      const auto walletInfo = response.wallets(i);
      result.push_back({ mapFrom(walletInfo.format()), walletInfo.id(), walletInfo.name()
         , walletInfo.description(), mapFrom(walletInfo.nettype()), walletInfo.watching_only() });
      if (walletInfo.watching_only()) {
         woWallets_.insert(walletInfo.id());
      }
   }
   itCb->second(result);
   cbWalletInfoMap_.erase(itCb);
}

void HeadlessContainer::ProcessSyncHDWallet(unsigned int id, const std::string &data)
{
   headless::SyncHDWalletResponse response;
   if (!response.ParseFromString(data)) {
      logger_->error("[{}] Failed to parse reply", __func__);
      emit Error(id, "failed to parse");
      return;
   }
   const auto itCb = cbHDWalletMap_.find(id);
   if (itCb == cbHDWalletMap_.end()) {
      emit Error(id, "no callback found for id " + std::to_string(id));
      return;
   }
   const bool isWoRoot = (woWallets_.find(response.walletid()) != woWallets_.end());
   bs::sync::HDWalletData result;
   for (int i = 0; i < response.groups_size(); ++i) {
      const auto groupInfo = response.groups(i);
      bs::sync::HDWalletData::Group group;
      group.type = static_cast<bs::hd::CoinType>(groupInfo.type());
      for (int j = 0; j < groupInfo.leaves_size(); ++j) {
         const auto leafInfo = groupInfo.leaves(j);
         if (isWoRoot) {
            woWallets_.insert(leafInfo.id());
         }
         group.leaves.push_back({ leafInfo.id(), leafInfo.index() });
      }
      result.groups.push_back(group);
   }
   itCb->second(result);
   cbHDWalletMap_.erase(itCb);
}

static bs::wallet::EncryptionType mapFrom(headless::EncryptionType encType)
{
   switch (encType) {
   case headless::EncryptionTypePassword: return bs::wallet::EncryptionType::Password;
   case headless::EncryptionTypeAutheID:  return bs::wallet::EncryptionType::Auth;
   case headless::EncryptionTypeUnencrypted:
   default:    return bs::wallet::EncryptionType::Unencrypted;
   }
}

void HeadlessContainer::ProcessSyncWallet(unsigned int id, const std::string &data)
{
   headless::SyncWalletResponse response;
   if (!response.ParseFromString(data)) {
      logger_->error("[{}] Failed to parse reply", __func__);
      emit Error(id, "failed to parse");
      return;
   }
   const auto itCb = cbWalletMap_.find(id);
   if (itCb == cbWalletMap_.end()) {
      emit Error(id, "no callback found for id " + std::to_string(id));
      return;
   }

   bs::sync::WalletData result;
   for (int i = 0; i < response.encryptiontypes_size(); ++i) {
      const auto encType = response.encryptiontypes(i);
      result.encryptionTypes.push_back(mapFrom(encType));
   }
   for (int i = 0; i < response.encryptionkeys_size(); ++i) {
      const auto encKey = response.encryptionkeys(i);
      result.encryptionKeys.push_back(encKey);
   }
   result.encryptionRank = { response.keyrank().m(), response.keyrank().n() };

   for (int i = 0; i < response.addresses_size(); ++i) {
      const auto addrInfo = response.addresses(i);
      const bs::Address addr(addrInfo.address());
      if (addr.isNull()) {
         continue;
      }
      result.addresses.push_back({ addrInfo.index(), std::move(addr)
         , addrInfo.comment() });
   }
   for (int i = 0; i < response.addrpool_size(); ++i) {
      const auto addrInfo = response.addrpool(i);
      const bs::Address addr(addrInfo.address());
      if (addr.isNull()) {
         continue;
      }
      result.addrPool.push_back({ addrInfo.index(), std::move(addr), "" });
   }
   for (int i = 0; i < response.txcomments_size(); ++i) {
      const auto txInfo = response.txcomments(i);
      result.txComments.push_back({ txInfo.txhash(), txInfo.comment() });
   }
   itCb->second(result);
   cbWalletMap_.erase(itCb);
}

void HeadlessContainer::ProcessSyncAddresses(unsigned int id, const std::string &data)
{
   headless::SyncAddressesResponse response;
   if (!response.ParseFromString(data)) {
      logger_->error("[{}] Failed to parse reply", __func__);
      emit Error(id, "failed to parse");
      return;
   }
   const auto itCb = cbNewAddrsMap_.find(id);
   if (itCb == cbNewAddrsMap_.end()) {
      logger_->error("[{}] no callback found for id {}", __func__, id);
      emit Error(id, "no callback found for id " + std::to_string(id));
      return;
   }

   std::vector<std::pair<bs::Address, std::string>> result;
   for (int i = 0; i < response.addresses_size(); ++i) {
      const auto addrInfo = response.addresses(i);
      try {
         const bs::Address addr(addrInfo.address());
         if (addr.isNull()) {
            logger_->debug("[{}] addr #{} is null", __func__, i);
            continue;
         }
         result.push_back({ std::move(addr), addrInfo.index() });
      }
      catch (const std::exception &e) {
         logger_->error("[{}] failed to create address: {}", __func__, e.what());
      }
   }
   itCb->second(result);
   cbNewAddrsMap_.erase(itCb);
}


RemoteSigner::RemoteSigner(const std::shared_ptr<spdlog::logger> &logger
   , const QString &host, const QString &port, NetworkType netType
   , const std::shared_ptr<ConnectionManager>& connectionManager
   , const std::shared_ptr<ApplicationSettings>& appSettings
   , OpMode opMode
   , const bool ephemeralDataConnKeys
   , const std::string& ownKeyFileDir
   , const std::string& ownKeyFileName
   , const ZmqBIP15XDataConnection::cbNewKey& inNewKeyCB)
   : HeadlessContainer(logger, opMode)
   , host_(host), port_(port), netType_(netType)
   , ephemeralDataConnKeys_(ephemeralDataConnKeys)
   , ownKeyFileDir_(ownKeyFileDir)
   , ownKeyFileName_(ownKeyFileName)
   , appSettings_{appSettings}
   , cbNewKey_{inNewKeyCB}
   , connectionManager_{connectionManager}
{
   // Create connection upfront in order to grab some required data early.
   RecreateConnection();
}

// Establish the remote connection to the signer.
bool RemoteSigner::Start()
{
   // If we've already connected, don't do more setup.
   if (headlessConnFinished_) {
      return true;
   }

   if (opMode() == OpMode::RemoteInproc) {
      connection_->SetZMQTransport(ZMQTransport::InprocTransport);
   }

   {
      std::lock_guard<std::mutex> lock(mutex_);
      listener_ = std::make_shared<HeadlessListener>(logger_, connection_, netType_);
      connect(listener_.get(), &HeadlessListener::connected, this
         , &RemoteSigner::onConnected, Qt::QueuedConnection);
      connect(listener_.get(), &HeadlessListener::authenticated, this
         , &RemoteSigner::onAuthenticated, Qt::QueuedConnection);
      connect(listener_.get(), &HeadlessListener::disconnected, this
         , &RemoteSigner::onDisconnected, Qt::QueuedConnection);
      connect(listener_.get(), &HeadlessListener::error, this
         , &RemoteSigner::onConnError, Qt::QueuedConnection);
      connect(listener_.get(), &HeadlessListener::PacketReceived, this
         , &RemoteSigner::onPacketReceived, Qt::QueuedConnection);
   }

   return Connect();
}

bool RemoteSigner::Stop()
{
   return Disconnect();
}

bool RemoteSigner::Connect()
{
   QtConcurrent::run(this, &RemoteSigner::ConnectHelper);
   headlessConnFinished_ = true;
   return true;
}

void RemoteSigner::ConnectHelper()
{
   if (!connection_) {
      logger_->error("[{}] connection not created", __func__);
      emit disconnected();
      return;
   }
   if (!connection_->isActive()) {
      if (connection_->openConnection(host_.toStdString(), port_.toStdString()
         , listener_.get())) {
         emit connected();
      }
      else {
         logger_->error("[HeadlessContainer] Failed to open connection to "
            "headless container");
         return;
      }
   }
}

bool RemoteSigner::Disconnect()
{
   if (!connection_) {
      return true;
   }
/*   headless::RequestPacket packet;
   packet.set_type(headless::DisconnectionRequestType);
   packet.set_data("");    // This code produces crashes on terminal shutdown
   Send(packet);*/         // and its purpose is obscure to me

   return connection_->closeConnection();
}

void RemoteSigner::Authenticate()
{
   mutex_.lock();
   if (!listener_) {
      mutex_.unlock();
      emit connectionError(UnknownError, tr("listener missing on authenticate"));
      return;
   }
   mutex_.unlock();

   headless::AuthenticationRequest request;
   request.set_nettype((netType_ == NetworkType::TestNet) ? headless::TestNetType : headless::MainNetType);

   headless::RequestPacket packet;
   packet.set_type(headless::AuthenticationRequestType);
   packet.set_data(request.SerializeAsString());
   Send(packet);
}

void RemoteSigner::RecreateConnection()
{
   logger_->info("[{}] Restart connection...", __func__);

   const bool makeClientCookie = false;
   // Server's cookies are not available in remote mode
   const bool readServerCookie = (opMode() == OpMode::Local || opMode() == OpMode::LocalInproc);

   std::string absCookiePath;
   if (readServerCookie) {
      absCookiePath = SystemFilePaths::appDataLocation() + "/" + "signerServerID";
   }

   try {
      connection_ = connectionManager_->CreateZMQBIP15XDataConnection(
         ephemeralDataConnKeys_, ownKeyFileDir_, ownKeyFileName_, makeClientCookie
         , readServerCookie, absCookiePath);
      connection_->setCBs(cbNewKey_);
      connection_->setLocalHeartbeatInterval();

      headlessConnFinished_ = false;
   }
   catch (const std::exception &e) {
      logger_->error("[{}] connection creation failed: {}", __func__, e.what());
      QTimer::singleShot(10, [this] {  // slight delay is required on start-up init
         emit connectionError(ConnectionError::SocketFailed, tr("Connection creation failed"));
      });
   }
}

void RemoteSigner::ScheduleRestart()
{
   if (isRestartScheduled_) {
      return;
   }

   isRestartScheduled_ = true;
   QTimer::singleShot(kRemoteReconnectPeriod, this, [this] {
      isRestartScheduled_ = false;
      RecreateConnection();
      Start();
   });
}

bool RemoteSigner::isOffline() const
{
   std::lock_guard<std::mutex> lock(mutex_);
   return (listener_ == nullptr);
}

bool RemoteSigner::hasUI() const
{
   std::lock_guard<std::mutex> lock(mutex_);

   return listener_ ? listener_->hasUI() : false;
}

void RemoteSigner::onConnected()
{
   Authenticate();
}

void RemoteSigner::onAuthenticated()
{
   // Once the BIP 150/151 handshake is complete, it's safe to start sending
   // app-level data to the signer.
   emit authenticated();
   emit ready();
}

void RemoteSigner::onDisconnected()
{
   missingWallets_.clear();
   woWallets_.clear();

   // signRequests_ will be empty after that
   std::set<bs::signer::RequestId> tmpReqs = std::move(signRequests_);

   for (const auto &id : tmpReqs) {
      emit TXSigned(id, {}, "signer disconnected", false);
   }

   emit disconnected();

   ScheduleRestart();
}

void RemoteSigner::onConnError(ConnectionError error, const QString &details)
{
   emit connectionError(error, details);
   ScheduleRestart();
}

void RemoteSigner::onPacketReceived(headless::RequestPacket packet)
{
   signRequests_.erase(packet.id());

   switch (packet.type()) {
   case headless::HeartbeatType:
      break;

   case headless::SignTXRequestType:
   case headless::SignPartialTXRequestType:
   case headless::SignPayoutTXRequestType:
   case headless::SignTXMultiRequestType:
      ProcessSignTXResponse(packet.id(), packet.data());
      break;

   case headless::PasswordRequestType:
      ProcessPasswordRequest(packet.data());
      break;

   case headless::CreateHDWalletRequestType:
      ProcessCreateHDWalletResponse(packet.id(), packet.data());
      break;

   case headless::GetRootKeyRequestType:
      ProcessGetRootKeyResponse(packet.id(), packet.data());
      break;

   case headless::GetHDWalletInfoRequestType:
      ProcessGetHDWalletInfoResponse(packet.id(), packet.data());
      break;

   case headless::SetUserIdRequestType:
      emit UserIdSet();
      break;

   case headless::SetLimitsRequestType:
      ProcessSetLimitsResponse(packet.id(), packet.data());
      break;

   case headless::CreateSettlWalletType:
      ProcessSettlWalletCreate(packet.id(), packet.data());
      break;

   case headless::SyncWalletInfoType:
      ProcessSyncWalletInfo(packet.id(), packet.data());
      break;

   case headless::SyncHDWalletType:
      ProcessSyncHDWallet(packet.id(), packet.data());
      break;

   case headless::SyncWalletType:
      ProcessSyncWallet(packet.id(), packet.data());
      break;

   case headless::SyncCommentType:
      break;   // normally no data will be returned on sync of comments

   case headless::SyncAddressesType:
      ProcessSyncAddresses(packet.id(), packet.data());
      break;

   case headless::WalletsListUpdatedType:
      logger_->debug("received WalletsListUpdatedType message");
      emit walletsListUpdated();
      break;

   default:
      logger_->warn("[HeadlessContainer] Unknown packet type: {}", packet.type());
      break;
   }
}

void RemoteSigner::setTargetDir(const QString& targetDir)
{
   appSettings_->set(ApplicationSettings::signerOfflineDir, targetDir);
}

QString RemoteSigner::targetDir() const
{
   return appSettings_->get<QString>(ApplicationSettings::signerOfflineDir);
}

bs::signer::RequestId RemoteSigner::signTXRequest(const bs::core::wallet::TXSignRequest &txSignReq
   , bool autoSign, SignContainer::TXSignMode mode, const PasswordType& password
   , bool keepDuplicatedRecipients)
{
   if (isWalletOffline(txSignReq.walletId)) {
      return signOffline(txSignReq);
   }
   return HeadlessContainer::signTXRequest(txSignReq, autoSign, mode, password, keepDuplicatedRecipients);
}

bs::signer::RequestId RemoteSigner::signOffline(const bs::core::wallet::TXSignRequest &txSignReq)
{
   if (!txSignReq.isValid()) {
      logger_->error("[HeadlessContainer] Invalid TXSignRequest");
      return 0;
   }

   Blocksettle::Storage::Signer::TXRequest request;
   request.set_walletid(txSignReq.walletId);

   for (const auto &utxo : txSignReq.inputs) {
      auto input = request.add_inputs();
      input->set_utxo(utxo.serialize().toBinStr());
      const auto addr = bs::Address::fromUTXO(utxo);
      input->mutable_address()->set_address(addr.display());
   }

   for (const auto &recip : txSignReq.recipients) {
      request.add_recipients(recip->getSerializedScript().toBinStr());
   }

   if (txSignReq.fee) {
      request.set_fee(txSignReq.fee);
   }
   if (txSignReq.RBF) {
      request.set_rbf(true);
   }

   if (txSignReq.change.value) {
      auto change = request.mutable_change();
      change->mutable_address()->set_address(txSignReq.change.address.display());
      change->mutable_address()->set_index(txSignReq.change.index);
      change->set_value(txSignReq.change.value);
   }

   if (!txSignReq.comment.empty()) {
      request.set_comment(txSignReq.comment);
   }

   Blocksettle::Storage::Signer::File fileContainer;
   auto container = fileContainer.add_payload();
   container->set_type(Blocksettle::Storage::Signer::RequestFileType);
   container->set_data(request.SerializeAsString());

   const auto timestamp = std::to_string(QDateTime::currentDateTime().toSecsSinceEpoch());
   const auto targetDir = appSettings_->get<std::string>(ApplicationSettings::signerOfflineDir);
   const std::string fileName = targetDir + "/" + txSignReq.walletId + "_" + timestamp + ".bin";

   const auto reqId = listener_->newRequestId();
   QFile f(QString::fromStdString(fileName));
   if (f.exists()) {
      QMetaObject::invokeMethod(this, [this, reqId, fileName] {
         emit TXSigned(reqId, {}, "request file " + fileName + " already exists", false);
      });
      return reqId;
   }
   if (!f.open(QIODevice::WriteOnly)) {
      QMetaObject::invokeMethod(this, [this, reqId, fileName] {
         emit TXSigned(reqId, {}, "failed to open " + fileName + " for writing", false);
      });
      return reqId;
   }

   const auto data = QByteArray::fromStdString(fileContainer.SerializeAsString());
   if (f.write(data) != data.size()) {
      QMetaObject::invokeMethod(this, [this, reqId, fileName] {
         emit TXSigned(reqId, {}, "failed to write to " + fileName, false);
      });
      return reqId;
   }
   f.close();

   QMetaObject::invokeMethod(this, [this, reqId, fileName] {
      emit TXSigned(reqId, fileName, {}, false);
   });
   return reqId;
}


LocalSigner::LocalSigner(const std::shared_ptr<spdlog::logger> &logger
   , const QString &homeDir, NetworkType netType, const QString &port
   , const std::shared_ptr<ConnectionManager>& connectionManager
   , const std::shared_ptr<ApplicationSettings> &appSettings
   , const bool startSignerProcess
   , const std::string& ownKeyFileDir
   , const std::string& ownKeyFileName
   , double asSpendLimit
   , const ZmqBIP15XDataConnection::cbNewKey& inNewKeyCB)
   : RemoteSigner(logger, QLatin1String("127.0.0.1"), port, netType
      , connectionManager, appSettings, OpMode::Local, true
      , ownKeyFileDir, ownKeyFileName, inNewKeyCB)
      , homeDir_(homeDir), startProcess_(startSignerProcess), asSpendLimit_(asSpendLimit)
{}

LocalSigner::~LocalSigner() noexcept
{
   Stop();
}

QStringList LocalSigner::args() const
{
   auto walletsCopyDir = homeDir_ + QLatin1String("/copy");
   if (!QDir().exists(walletsCopyDir)) {
      walletsCopyDir = homeDir_ + QLatin1String("/signer");
   }

   QStringList result;
   result << QLatin1String("--guimode") << QLatin1String("lightgui");
   switch (netType_) {
   case NetworkType::TestNet:
   case NetworkType::RegTest:
      result << QString::fromStdString("--testnet");
      break;
   case NetworkType::MainNet:
      result << QString::fromStdString("--mainnet");
      break;
   default:
      break;
   }

   // Among many other things, send the signer the terminal's BIP 150 ID key.
   // Processes reading keys from the disk are subject to attack.
   result << QLatin1String("--listen") << QLatin1String("127.0.0.1");
   result << QLatin1String("--port") << port_;
   result << QLatin1String("--dirwallets") << walletsCopyDir;
   if (asSpendLimit_ > 0) {
      result << QLatin1String("--auto_sign_spend_limit")
         << QString::number(asSpendLimit_, 'f', 8);
   }
   result << QLatin1String("--terminal_id_key")
      << QString::fromStdString(connection_->getOwnPubKey().toHexStr());

   return result;
}

bool LocalSigner::Start()
{
   Stop();

   if (startProcess_) {
      // If there's a previous headless process, stop it.
      headlessProcess_ = std::make_shared<QProcess>();

#ifdef Q_OS_WIN
      const auto signerAppPath = QCoreApplication::applicationDirPath() + QLatin1String("/blocksettle_signer.exe");
#elif defined (Q_OS_MACOS)
      auto bundleDir = QDir(QCoreApplication::applicationDirPath());
      bundleDir.cdUp();
      bundleDir.cdUp();
      bundleDir.cdUp();
      const auto signerAppPath = bundleDir.absoluteFilePath(QLatin1String("blocksettle_signer"));
#else
      const auto signerAppPath = QCoreApplication::applicationDirPath() + QLatin1String("/blocksettle_signer");
#endif
      if (!QFile::exists(signerAppPath)) {
         logger_->error("[HeadlessContainer] Signer binary {} not found"
            , signerAppPath.toStdString());
         emit connectionError(UnknownError, tr("missing signer binary"));
         return false;
      }

      const auto cmdArgs = args();
      logger_->debug("[HeadlessContainer] starting {} {}"
         , signerAppPath.toStdString(), cmdArgs.join(QLatin1Char(' ')).toStdString());

#ifndef NDEBUG
      headlessProcess_->setProcessChannelMode(QProcess::MergedChannels);
      connect(headlessProcess_.get(), &QProcess::readyReadStandardOutput, this, [this]() {
         qDebug().noquote() << headlessProcess_->readAllStandardOutput();
      });
#endif

      headlessProcess_->start(signerAppPath, cmdArgs);
      if (!headlessProcess_->waitForStarted(kStartTimeout)) {
         logger_->error("[HeadlessContainer] Failed to start process");
         headlessProcess_.reset();
         emit connectionError(UnknownError, tr("failed to start process"));
         return false;
      }

      // Give the signer a little time to get set up.
      QThread::msleep(250);
   }

   return RemoteSigner::Start();
}

bool LocalSigner::Stop()
{
   RemoteSigner::Stop();

   if (headlessProcess_) {
      if (!headlessProcess_->waitForFinished(kKillTimeout)) {
         headlessProcess_->terminate();
         headlessProcess_->waitForFinished(kKillTimeout);
      }
      headlessProcess_.reset();
   }
   return true;
}

//#include "HeadlessContainer.moc"
