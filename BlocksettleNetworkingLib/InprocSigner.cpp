#include "InprocSigner.h"
#include <spdlog/spdlog.h>
#include "Address.h"
#include "CoreWalletsManager.h"
#include "CoreHDWallet.h"

InprocSigner::InprocSigner(const std::shared_ptr<bs::core::WalletsManager> &mgr
   , const std::shared_ptr<spdlog::logger> &logger, const std::string &walletsPath
   , NetworkType netType)
   : SignContainer(logger, SignContainer::OpMode::LocalInproc)
   , walletsMgr_(mgr), walletsPath_(walletsPath), netType_(netType)
{ }

bool InprocSigner::Start()
{
   const auto &cbLoadProgress = [this](int cur, int total) {
      logger_->debug("[InprocSigner::Start] loading wallets: {} of {}", cur, total);
   };
   walletsMgr_->loadWallets(netType_, walletsPath_, cbLoadProgress);
   emit ready();
   return true;
}

SignContainer::RequestId InprocSigner::SignTXRequest(const bs::wallet::TXSignRequest &txSignReq,
   bool, TXSignMode mode, const PasswordType &password, bool)
{
   if (!txSignReq.isValid()) {
      logger_->error("[{}] Invalid TXSignRequest", __func__);
      return 0;
   }
   const auto wallet = walletsMgr_->getWalletById(txSignReq.walletId);
   if (!wallet) {
      logger_->error("[{}] failed to find wallet with id {}", __func__, txSignReq.walletId);
      return 0;
   }

   const auto reqId = seqId_++;
   bs::core::wallet::TXSignRequest req;
   req.inputs = txSignReq.inputs;
   req.recipients = txSignReq.recipients;
   req.change.address = txSignReq.change.address;
   req.change.index = txSignReq.change.index;
   req.change.value = txSignReq.change.value;
   req.fee = txSignReq.fee;
   req.comment = txSignReq.comment;
   req.RBF = txSignReq.RBF;
   req.walletId = txSignReq.walletId;
   try {
      BinaryData signedTx;
      if (mode == TXSignMode::Full) {
         signedTx = wallet->signTXRequest(req, password);
      } else {
         signedTx = wallet->signPartialTXRequest(req, password);
      }
      QTimer::singleShot(1, [this, reqId, signedTx] {emit TXSigned(reqId, signedTx, {}, false); });
   }
   catch (const std::exception &e) {
      QTimer::singleShot(1, [this, reqId, e] { emit TXSigned(reqId, {}, e.what(), false); });
   }
   return reqId;  //stub
}

SignContainer::RequestId InprocSigner::CreateHDWallet(const std::string &name, const std::string &desc
   , bool primary, const bs::core::wallet::Seed &seed, const std::vector<bs::wallet::PasswordData> &pwdData
   , bs::wallet::KeyRank keyRank)
{
   const auto wallet = walletsMgr_->createWallet(name, desc, seed, walletsPath_, primary, pwdData, keyRank);
   const RequestId reqId = seqId_++;
   const auto hdWallet = std::make_shared<bs::hd::Wallet>(wallet->walletId(), seed.networkType(), false
      , wallet->name(), logger_, wallet->description());
   QTimer::singleShot(1, [this, reqId, hdWallet] { emit HDWalletCreated(reqId, hdWallet); });
   return reqId;  //stub
}

void InprocSigner::syncWalletInfo(const std::function<void(std::vector<bs::sync::WalletInfo>)> &cb)
{
   std::vector<bs::sync::WalletInfo> result;
   for (size_t i = 0; i < walletsMgr_->getHDWalletsCount(); ++i) {
      const auto hdWallet = walletsMgr_->getHDWallet(i);
      result.push_back({ bs::sync::WalletFormat::HD, hdWallet->walletId(), hdWallet->name()
         , hdWallet->description(), hdWallet->networkType() });
   }
   const auto settlWallet = walletsMgr_->getSettlementWallet();
   if (settlWallet) {
      result.push_back({ bs::sync::WalletFormat::Settlement, settlWallet->walletId(), settlWallet->name()
         , settlWallet->description(), settlWallet->networkType() });
   }
   cb(result);
}

void InprocSigner::syncHDWallet(const std::string &id, const std::function<void(bs::sync::HDWalletData)> &cb)
{
   bs::sync::HDWalletData result;
   const auto hdWallet = walletsMgr_->getHDWalletById(id);
   if (hdWallet) {
      for (const auto &group : hdWallet->getGroups()) {
         bs::sync::HDWalletData::Group groupData;
         groupData.type = static_cast<bs::hd::CoinType>(group->index());

         for (const auto &leaf : group->getLeaves()) {
            groupData.leaves.push_back({ leaf->walletId(), leaf->index() });
         }
         result.groups.push_back(groupData);
      }
   }
   else {
      logger_->error("[{}] failed to find HD wallet with id {}", __func__, id);
   }
   cb(result);
}

void InprocSigner::syncWallet(const std::string &id, const std::function<void(bs::sync::WalletData)> &cb)
{
   bs::sync::WalletData result;
   const auto wallet = walletsMgr_->getWalletById(id);
   if (wallet) {
      result.isWatchingOnly = wallet->isWatchingOnly();
      result.encryptionTypes = wallet->encryptionTypes();
      result.encryptionKeys = wallet->encryptionKeys();
      result.encryptionRank = wallet->encryptionRank();
      result.netType = wallet->networkType();

      for (const auto &addr : wallet->getUsedAddressList()) {
         const auto index = wallet->getAddressIndex(addr);
         result.addresses.push_back({index, addr.getType(), addr});
      }
      //TODO: same for pooled addresses
   }
   cb(result);
}
