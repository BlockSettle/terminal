#include "InprocSigner.h"
#include <QTimer>
#include <spdlog/spdlog.h>
#include "Address.h"
#include "CoreSettlementWallet.h"
#include "CoreWalletsManager.h"
#include "CoreHDWallet.h"
#include "Wallets/SyncHDWallet.h"
#include "Wallets/SyncSettlementWallet.h"

InprocSigner::InprocSigner(const std::shared_ptr<bs::core::WalletsManager> &mgr
   , const std::shared_ptr<spdlog::logger> &logger, const std::string &walletsPath
   , NetworkType netType)
   : SignContainer(logger, SignContainer::OpMode::LocalInproc)
   , walletsMgr_(mgr), walletsPath_(walletsPath), netType_(netType)
{ }

InprocSigner::InprocSigner(const std::shared_ptr<bs::core::hd::Wallet> &wallet
   , const std::shared_ptr<spdlog::logger> &logger)
   : SignContainer(logger, SignContainer::OpMode::LocalInproc)
   , walletsPath_({}), netType_(wallet->networkType())
{
   walletsMgr_ = std::make_shared<bs::core::WalletsManager>(logger);
   walletsMgr_->addWallet(wallet, walletsPath_);
}

InprocSigner::InprocSigner(const std::shared_ptr<bs::core::SettlementWallet> &wallet
   , const std::shared_ptr<spdlog::logger> &logger)
   : SignContainer(logger, SignContainer::OpMode::LocalInproc)
   , walletsPath_({}), netType_(wallet->networkType())
{
   walletsMgr_ = std::make_shared<bs::core::WalletsManager>(logger);
   walletsMgr_->setSettlementWallet(wallet);
}

bool InprocSigner::Start()
{
   if (!walletsPath_.empty() && !walletsMgr_->walletsLoaded()) {
      const auto &cbLoadProgress = [this](int cur, int total) {
         logger_->debug("[InprocSigner::Start] loading wallets: {} of {}", cur, total);
      };
      walletsMgr_->loadWallets(netType_, walletsPath_, cbLoadProgress);
   }
   inited_ = true;
   emit ready();
   return true;
}

// All signing code below doesn't include password request support for encrypted wallets - i.e.
// a password should be passed directly to signing methods

bs::signer::RequestId InprocSigner::signTXRequest(const bs::core::wallet::TXSignRequest &txSignReq,
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
   try {
      BinaryData signedTx;
      if (mode == TXSignMode::Full) {
         signedTx = wallet->signTXRequest(txSignReq, password);
      } else {
         signedTx = wallet->signPartialTXRequest(txSignReq, password);
      }
      QTimer::singleShot(1, [this, reqId, signedTx] {emit TXSigned(reqId, signedTx, {}, false); });
   }
   catch (const std::exception &e) {
      QTimer::singleShot(1, [this, reqId, e] { emit TXSigned(reqId, {}, e.what(), false); });
   }
   return reqId;
}

bs::signer::RequestId InprocSigner::signPartialTXRequest(const bs::core::wallet::TXSignRequest &txReq
   , bool autoSign, const PasswordType &password)
{
   return signTXRequest(txReq, autoSign, TXSignMode::Partial, password);
}

bs::signer::RequestId InprocSigner::signPayoutTXRequest(const bs::core::wallet::TXSignRequest &txSignReq
   , const bs::Address &authAddr, const std::string &settlementId, bool autoSign, const PasswordType &password)
{
   if (!txSignReq.isValid()) {
      logger_->error("[{}] Invalid TXSignRequest", __func__);
      return 0;
   }
   const auto settlWallet = std::dynamic_pointer_cast<bs::core::SettlementWallet>(walletsMgr_->getSettlementWallet());
   if (!settlWallet) {
      logger_->error("[{}] failed to find settlement wallet", __func__);
      return 0;
   }
   const auto authWallet = walletsMgr_->getAuthWallet();
   if (!authWallet) {
      logger_->error("[{}] failed to find auth wallet", __func__);
      return 0;
   }
   const auto authKeys = authWallet->getKeyPairFor(authAddr, password);
   if (authKeys.privKey.isNull() || authKeys.pubKey.isNull()) {
      logger_->error("[{}] failed to get priv/pub keys for {}", __func__, authAddr.display());
      return 0;
   }

   const auto reqId = seqId_++;
   try {
      const auto signedTx = settlWallet->signPayoutTXRequest(txSignReq, authKeys, settlementId);
      QTimer::singleShot(1, [this, reqId, signedTx] {emit TXSigned(reqId, signedTx, {}, false); });
   } catch (const std::exception &e) {
      QTimer::singleShot(1, [this, reqId, e] { emit TXSigned(reqId, {}, e.what(), false); });
   }
   return reqId;
}

bs::signer::RequestId InprocSigner::signMultiTXRequest(const bs::core::wallet::TXMultiSignRequest &)
{
   logger_->info("[{}] currently not supported", __func__);
   return 0;
}

bs::signer::RequestId InprocSigner::createHDLeaf(const std::string &rootWalletId, const bs::hd::Path &path
   , const std::vector<bs::wallet::PasswordData> &pwdData)
{
   const auto hdWallet = walletsMgr_->getHDWalletById(rootWalletId);
   if (!hdWallet) {
      logger_->error("[{}] failed to get HD wallet by id {}", __func__, rootWalletId);
      return 0;
   }
   if (path.length() < 3) {
      logger_->error("[{}] too short path: {}", __func__, path.toString());
      return 0;
   }
   const auto groupType = static_cast<bs::hd::CoinType>(path.get(-2));
   const auto group = hdWallet->createGroup(groupType);
   if (!group) {
      logger_->error("[{}] failed to create/get group for {}", __func__, path.get(-2));
      return 0;
   }

   if (!walletsPath_.empty()) {
      walletsMgr_->backupWallet(hdWallet, walletsPath_);
   }

   std::shared_ptr<bs::core::hd::Leaf> leaf;
   SecureBinaryData password;
   for (const auto &pwd : pwdData) {
      password = mergeKeys(password, pwd.password);
   }

   try {
      std::shared_ptr<bs::core::hd::Node> leafNode;
      const auto &rootNode = hdWallet->getRootNode(password);
      if (rootNode) {
         leafNode = rootNode->derive(path);
      } else {
         logger_->error("[{}] failed to decrypt root node", __func__);
         return 0;
      }

      const auto leafIndex = path.get(2);
      leaf = group->createLeaf(leafIndex, leafNode);
      if (!leaf || !(leaf = group->getLeaf(leafIndex))) {
         logger_->error("[{}] failed to create/get leaf {}", __func__, path.toString());
         return 0;
      }
   }
   catch (const std::exception &e) {
      logger_->error("[{}] failed to decrypt root node {}", __func__, rootWalletId);
      return 0;
   }

   const bs::signer::RequestId reqId = seqId_++;
   std::shared_ptr<bs::sync::hd::Leaf> hdLeaf;
   switch (groupType) {
   case bs::hd::CoinType::Bitcoin_main:
   case bs::hd::CoinType::Bitcoin_test:
      hdLeaf = std::make_shared<bs::sync::hd::Leaf>(leaf->walletId(), leaf->name(), leaf->description()
         , this, logger_, leaf->type(), leaf->hasExtOnlyAddresses());
      break;
   case bs::hd::CoinType::BlockSettle_Auth:
      hdLeaf = std::make_shared<bs::sync::hd::AuthLeaf>(leaf->walletId(), leaf->name()
         , leaf->description(), this, logger_);
      break;
   case bs::hd::CoinType::BlockSettle_CC:
      hdLeaf = std::make_shared<bs::sync::hd::CCLeaf>(leaf->walletId(), leaf->name()
         , leaf->description(), this, logger_, leaf->hasExtOnlyAddresses());
      break;
   default:    break;
   }
   QTimer::singleShot(1, [this, reqId, hdLeaf] {emit HDLeafCreated(reqId, hdLeaf); });
   return reqId;
}

bs::signer::RequestId InprocSigner::createHDWallet(const std::string &name, const std::string &desc
   , bool primary, const bs::core::wallet::Seed &seed, const std::vector<bs::wallet::PasswordData> &pwdData
   , bs::wallet::KeyRank keyRank)
{
   try {
      const auto wallet = walletsMgr_->createWallet(name, desc, seed, walletsPath_, primary, pwdData, keyRank);
      const bs::signer::RequestId reqId = seqId_++;
      const auto hdWallet = std::make_shared<bs::sync::hd::Wallet>(wallet->networkType(), wallet->walletId()
         , wallet->name(), wallet->description(), this, logger_);
      QTimer::singleShot(1, [this, reqId, hdWallet] { emit HDWalletCreated(reqId, hdWallet); });
      return reqId;
   }
   catch (const std::exception &e) {
      logger_->error("[{}] failed to create HD wallet: {}", __func__, e.what());
   }
   return 0;
}

void InprocSigner::createSettlementWallet(const std::function<void(const std::shared_ptr<bs::sync::SettlementWallet> &)> &cb)
{
   auto wallet = walletsMgr_->getSettlementWallet();
   if (!wallet) {
      wallet = walletsMgr_->createSettlementWallet(netType_, walletsPath_);
   }
   const auto settlWallet = std::make_shared<bs::sync::SettlementWallet>(wallet->walletId(), wallet->name()
      , wallet->description(), this, logger_);
   if (cb) {
      cb(settlWallet);
   }
}

bs::signer::RequestId InprocSigner::customDialogRequest(bs::signer::ui::DialogType signerDialog, const QVariantMap &data)
{
   return 0;
}

bs::signer::RequestId InprocSigner::SetUserId(const BinaryData &userId)
{
   walletsMgr_->setChainCode(userId);
   QTimer::singleShot(1, [this] { emit UserIdSet(); });
   return seqId_++;
}

bs::signer::RequestId InprocSigner::DeleteHDRoot(const std::string &walletId)
{
   const auto wallet = walletsMgr_->getHDWalletById(walletId);
   if (!wallet) {
      logger_->error("[{}] failed to find wallet with id {}", __func__, walletId);
      return 0;
   }
   if (walletsMgr_->deleteWalletFile(wallet)) {
      return seqId_++;
   }
   return 0;
}

bs::signer::RequestId InprocSigner::DeleteHDLeaf(const std::string &walletId)
{
   const auto wallet = walletsMgr_->getWalletById(walletId);
   if (!wallet) {
      logger_->error("[{}] failed to find wallet with id {}", __func__, walletId);
      return 0;
   }
   if (walletsMgr_->deleteWalletFile(wallet)) {
      return seqId_++;
   }
   return 0;
}

//bs::signer::RequestId InprocSigner::changePassword(const std::string &walletId
//   , const std::vector<bs::wallet::PasswordData> &newPass
//   , bs::wallet::KeyRank keyRank, const SecureBinaryData &oldPass
//   , bool addNew, bool removeOld, bool dryRun)
//{
//   auto hdWallet = walletsMgr_->getHDWalletById(walletId);
//   if (!hdWallet) {
//      hdWallet = walletsMgr_->getHDRootForLeaf(walletId);
//      if (!hdWallet) {
//         logger_->error("[{}] failed to get wallet by id {}", __func__, walletId);
//         return 0;
//      }
//   }
//   const bool result = hdWallet->changePassword(newPass, keyRank, oldPass, addNew, removeOld, dryRun);
//   emit PasswordChanged(walletId, result);
//   return seqId_++;
//}

bs::signer::RequestId InprocSigner::GetInfo(const std::string &walletId)
{
   auto hdWallet = walletsMgr_->getHDWalletById(walletId);
   if (!hdWallet) {
      hdWallet = walletsMgr_->getHDRootForLeaf(walletId);
      if (!hdWallet) {
         logger_->error("[{}] failed to get wallet by id {}", __func__, walletId);
         return 0;
      }
   }
   const auto reqId = seqId_++;
   const bs::hd::WalletInfo walletInfo(hdWallet);
   QTimer::singleShot(1, [this, reqId, walletInfo] { emit QWalletInfo(reqId, walletInfo); });
   return reqId;
}

void InprocSigner::syncWalletInfo(const std::function<void(std::vector<bs::sync::WalletInfo>)> &cb)
{
   std::vector<bs::sync::WalletInfo> result;
   for (size_t i = 0; i < walletsMgr_->getHDWalletsCount(); ++i) {
      const auto hdWallet = walletsMgr_->getHDWallet(i);
      result.push_back({ bs::sync::WalletFormat::HD, hdWallet->walletId(), hdWallet->name()
         , hdWallet->description(), hdWallet->networkType(), hdWallet->isWatchingOnly() });
   }
   const auto settlWallet = walletsMgr_->getSettlementWallet();
   if (settlWallet) {
      result.push_back({ bs::sync::WalletFormat::Settlement, settlWallet->walletId(), settlWallet->name()
         , settlWallet->description(), settlWallet->networkType(), true });
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
      result.encryptionTypes = wallet->encryptionTypes();
      result.encryptionKeys = wallet->encryptionKeys();
      result.encryptionRank = wallet->encryptionRank();
      result.netType = wallet->networkType();

      for (const auto &addr : wallet->getUsedAddressList()) {
         const auto index = wallet->getAddressIndex(addr);
         const auto comment = wallet->getAddressComment(addr);
         result.addresses.push_back({index, addr, comment});
      }
      for (const auto &addr : wallet->getPooledAddressList()) {
         const auto index = wallet->getAddressIndex(addr);
         result.addrPool.push_back({ index, addr, ""});
      }
      for (const auto &txComment : wallet->getAllTxComments()) {
         result.txComments.push_back({txComment.first, txComment.second});
      }
   }
   cb(result);
}

void InprocSigner::syncAddressComment(const std::string &walletId, const bs::Address &addr, const std::string &comment)
{
   const auto wallet = walletsMgr_->getWalletById(walletId);
   if (wallet) {
      wallet->setAddressComment(addr, comment);
   }
}

void InprocSigner::syncTxComment(const std::string &walletId, const BinaryData &txHash, const std::string &comment)
{
   const auto wallet = walletsMgr_->getWalletById(walletId);
   if (wallet) {
      wallet->setTransactionComment(txHash, comment);
   }
}

void InprocSigner::syncNewAddress(const std::string &walletId, const std::string &index, AddressEntryType aet
   , const std::function<void(const bs::Address &)> &cb)
{
   const auto &cbAddrs = [cb](const std::vector<std::pair<bs::Address, std::string>> &outAddrs) {
      if (outAddrs.size() == 1) {
         cb(outAddrs[0].first);
      }
      else {
         cb({});
      }
   };
   syncNewAddresses(walletId, { {index, aet} }, cbAddrs);
}

void InprocSigner::syncNewAddresses(const std::string &walletId
   , const std::vector<std::pair<std::string, AddressEntryType>> &inData
   , const std::function<void(const std::vector<std::pair<bs::Address, std::string>> &)> &cb
   , bool persistent)
{
   std::vector<std::pair<bs::Address, std::string>> result;
   result.reserve(inData.size());
   const auto wallet = walletsMgr_->getWalletById(walletId);
   if (wallet) {
      for (const auto &in : inData) {
         std::string index;
         try {
            const bs::Address addr(in.first);
            if (addr.isValid()) {
               index = wallet->getAddressIndex(addr);
            }
         } catch (const std::exception &) {}
         if (index.empty()) {
            index = in.first;
         }
         result.push_back({ wallet->createAddressWithIndex(in.first, persistent, in.second), in.first });
      }
   }
   cb(result);
}
