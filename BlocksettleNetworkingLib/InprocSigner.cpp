#include "InprocSigner.h"
#include <QTimer>
#include <spdlog/spdlog.h>
#include "Address.h"
#include "CoreWalletsManager.h"
#include "CoreHDWallet.h"
#include "Wallets/SyncHDWallet.h"

InprocSigner::InprocSigner(const std::shared_ptr<bs::core::WalletsManager> &mgr
   , const std::shared_ptr<spdlog::logger> &logger, const std::string &walletsPath
   , NetworkType netType)
   : WalletSignerContainer(logger, SignContainer::OpMode::LocalInproc)
   , walletsMgr_(mgr), walletsPath_(walletsPath), netType_(netType)
{ }

InprocSigner::InprocSigner(const std::shared_ptr<bs::core::hd::Wallet> &wallet
   , const std::shared_ptr<spdlog::logger> &logger)
   : WalletSignerContainer(logger, SignContainer::OpMode::LocalInproc)
   , walletsPath_({}), netType_(wallet->networkType())
{
   walletsMgr_ = std::make_shared<bs::core::WalletsManager>(logger);
   walletsMgr_->addWallet(wallet);
}

InprocSigner::InprocSigner(const std::shared_ptr<bs::core::SettlementWallet> &wallet
   , const std::shared_ptr<spdlog::logger> &logger)
   : WalletSignerContainer(logger, SignContainer::OpMode::LocalInproc)
   , walletsPath_({}), netType_(wallet->networkType())
{
   walletsMgr_ = std::make_shared<bs::core::WalletsManager>(logger);
//   walletsMgr_->setSettlementWallet(wallet);
}

bool InprocSigner::Start()
{
   if (!walletsPath_.empty() && !walletsMgr_->walletsLoaded()) {
      const auto &cbLoadProgress = [this](size_t cur, size_t total) {
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
   TXSignMode mode, bool)
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
         signedTx = wallet->signTXRequest(txSignReq);
      } else {
         signedTx = wallet->signPartialTXRequest(txSignReq);
      }
      QTimer::singleShot(1, [this, reqId, signedTx] {
         emit TXSigned(reqId, signedTx, bs::error::ErrorCode::NoError);
      });
   }
   catch (const std::exception &e) {
      QTimer::singleShot(1, [this, reqId, e] {
         emit TXSigned(reqId, {}, bs::error::ErrorCode::InternalError, e.what());
      });
   }
   return reqId;
}

bs::signer::RequestId InprocSigner::signMultiTXRequest(const bs::core::wallet::TXMultiSignRequest &)
{
   logger_->info("[{}] currently not supported", __func__);
   return 0;
}

bs::signer::RequestId InprocSigner::signSettlementPayoutTXRequest(const bs::core::wallet::TXSignRequest &txReq
   , const bs::core::wallet::SettlementData &sd, const bs::sync::PasswordDialogData &
   , const std::function<void(bs::error::ErrorCode, const BinaryData &signedTX)> &cb)
{
   if (!txReq.isValid()) {
      logger_->error("[{}] Invalid payout TX sign request", __func__);
      return 0;
   }
   const auto wallet = walletsMgr_->getPrimaryWallet();
   if (!wallet) {
      logger_->error("[{}] failed to find primary wallet", __func__);
      return 0;
   }

   const auto reqId = seqId_++;
   try {
      const auto signedTx = wallet->signSettlementTXRequest(txReq, sd);
      QTimer::singleShot(1, [this, reqId, signedTx] {
         emit TXSigned(reqId, signedTx, bs::error::ErrorCode::NoError);
      });
      if (cb) {
         cb(bs::error::ErrorCode::NoError, signedTx);
      }
   } catch (const std::exception &e) {
      QTimer::singleShot(1, [this, reqId, e] {
         emit TXSigned(reqId, {}, bs::error::ErrorCode::InternalError, e.what());
      });
      if (cb) {
         cb(bs::error::ErrorCode::InternalError, {});
      }
   }
   return reqId;
}

bool InprocSigner::createHDLeaf(const std::string &rootWalletId, const bs::hd::Path &path
   , const std::vector<bs::wallet::PasswordData> &pwdData, bs::sync::PasswordDialogData
   , const std::function<void(bs::error::ErrorCode result)> &cb)
{
   const auto hdWallet = walletsMgr_->getHDWalletById(rootWalletId);
   if (!hdWallet) {
      logger_->error("[InprocSigner::createHDLeaf] failed to get HD wallet by id {}", rootWalletId);
      return false;
   }
   if (path.length() < 3) {
      logger_->error("[InprocSigner::createHDLeaf] too short path: {}", path.toString());
      return false;
   }
   const auto groupType = static_cast<bs::hd::CoinType>(path.get(-2));
   const auto group = hdWallet->createGroup(groupType);
   if (!group) {
      logger_->error("[InprocSigner::createHDLeaf] failed to create/get group for {}", path.get(-2));
      return false;
   }

   if (!walletsPath_.empty()) {
      walletsMgr_->backupWallet(hdWallet, walletsPath_);
   }

   std::shared_ptr<bs::core::hd::Leaf> leaf;

   try
   {
      const auto& password = pwdData[0].password;

      hdWallet->lockForEncryption(password);
      const auto leafIndex = path.get(2);
      auto leaf = group->createLeaf(leafIndex);
      if (leaf != nullptr) {
         return true;
      }
   }
   catch (const std::exception &) {
      logger_->error("[InprocSigner::createHDLeaf] failed to decrypt root node {}", rootWalletId);
      return false;
   }

   logger_->error("[InprocSigner::createHDLeaf] failed to create/get leaf {}", path.toString());
   return false;
}

void InprocSigner::createSettlementWallet(const bs::Address &authAddr
   , const std::function<void(const SecureBinaryData &)> &cb)
{
   const auto priWallet = walletsMgr_->getPrimaryWallet();
   if (!priWallet) {
      if (cb) {
         cb({});
      }
      return;
   }

   const auto leaf = priWallet->createSettlementLeaf(authAddr);
   if (!leaf) {
      if (cb) {
         cb({});
      }
      return;
   }
   const auto &cbWrap = [cb](bool, const SecureBinaryData &pubKey) {
      if (cb) {
         cb(pubKey);
      }
   };

   getRootPubkey(priWallet->walletId(), cbWrap);
}

bs::signer::RequestId InprocSigner::setUserId(const BinaryData &userId, const std::string &walletId)
{
   //walletsMgr_->setChainCode(userId);
   //TODO: add SetUserId implementation here
   return seqId_++;
}

bs::signer::RequestId InprocSigner::syncCCNames(const std::vector<std::string> &ccNames)
{
   walletsMgr_->setCCLeaves(ccNames);
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
   for (size_t i = 0; i < walletsMgr_->getHDWalletsCount(); ++i) 
   {
      const auto hdWallet = walletsMgr_->getHDWallet(i);
      result.push_back(
      { 
         bs::sync::WalletFormat::HD, 
         hdWallet->walletId(), hdWallet->name(), hdWallet->description(), 
         hdWallet->networkType(), hdWallet->isWatchingOnly() 
      });
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
         groupData.extOnly = group->isExtOnly();

         if (groupData.type == bs::hd::CoinType::BlockSettle_Auth) {
            auto authGroupPtr = 
               std::dynamic_pointer_cast<bs::core::hd::AuthGroup>(group);
            if (authGroupPtr == nullptr)
               throw std::runtime_error("unexpected group type");

            groupData.salt = authGroupPtr->getSalt();
         }

         for (const auto &leaf : group->getLeaves()) {
            BinaryData extraData;
            if (groupData.type == bs::hd::CoinType::BlockSettle_Settlement) {
               const auto settlLeaf = std::dynamic_pointer_cast<bs::core::hd::SettlementLeaf>(leaf);
               if (settlLeaf == nullptr) {
                  throw std::runtime_error("unexpected leaf type");
               }
               const auto rootAsset = settlLeaf->getRootAsset();
               const auto rootSingle = std::dynamic_pointer_cast<AssetEntry_Single>(rootAsset);
               if (rootSingle == nullptr) {
                  throw std::runtime_error("invalid root asset");
               }
               extraData = BtcUtils::getHash160(rootSingle->getPubKey()->getCompressedKey());
            }
            groupData.leaves.push_back(
               { leaf->walletId(), leaf->index(), leaf->hasExtOnlyAddresses(), std::move(extraData) });
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
   if (wallet) 
   {
      result.encryptionTypes = wallet->encryptionTypes();
      result.encryptionKeys = wallet->encryptionKeys();
      result.encryptionRank = wallet->encryptionRank();
      result.netType = wallet->networkType();
      
      result.highestExtIndex = wallet->getExtAddressCount();
      result.highestIntIndex = wallet->getIntAddressCount();

      size_t addrCnt = 0;
      for (const auto &addr : wallet->getUsedAddressList()) {
         const auto index = wallet->getAddressIndex(addr);
         const auto comment = wallet->getAddressComment(addr);
         result.addresses.push_back({index, addr, comment});
      }

      for (const auto &addr : wallet->getPooledAddressList()) {
         const auto index = wallet->getAddressIndex(addr);
         result.addrPool.push_back({ index, addr, ""});
      }

      for (const auto &txComment : wallet->getAllTxComments())
         result.txComments.push_back({txComment.first, txComment.second});
   }

   cb(result);
}

void InprocSigner::syncAddressComment(const std::string &walletId, const bs::Address &addr, const std::string &comment)
{
   const auto wallet = walletsMgr_->getWalletById(walletId);
   if (wallet)
      wallet->setAddressComment(addr, comment);
}

void InprocSigner::syncTxComment(const std::string &walletId, const BinaryData &txHash, const std::string &comment)
{
   const auto wallet = walletsMgr_->getWalletById(walletId);
   if (wallet)
      wallet->setTransactionComment(txHash, comment);
}

void InprocSigner::syncNewAddresses(const std::string &walletId
   , const std::vector<std::pair<std::string, AddressEntryType>> &inData
   , const std::function<void(const std::vector<std::pair<bs::Address, std::string>> &)> &cb
   , bool persistent)
{
   std::vector<std::pair<bs::Address, std::string>> result;
   const auto wallet = walletsMgr_->getWalletById(walletId);
   if (wallet == nullptr)
   {
      if (cb)
         cb(result);
      return;
   }

   result.reserve(inData.size());
   for (const auto &in : inData)
   {
      std::string index;
      try
      {
         const bs::Address addr(in.first);
         if (addr.isValid())
            index = wallet->getAddressIndex(addr);
      }
      catch (const std::exception&)
      {
      }

      if (index.empty())
         index = in.first;

      result.push_back({
         wallet->synchronizeUsedAddressChain(in.first, in.second).first, in.first });
   }

   if (cb)
      cb(result);
}

void InprocSigner::extendAddressChain(
   const std::string &walletId, unsigned count, bool extInt,
   const std::function<void(const std::vector<std::pair<bs::Address, std::string>> &)> &cb)
{
   /***
   Extend the wallet's account external (extInt == true) or internal 
   (extInt == false) chain, return the newly created addresses.

   These are not instantiated addresses, but pooled ones. They represent
   possible address type variations of the newly created assets, a set
   necessary to properly register the wallet with ArmoryDB.
   ***/

   std::vector<std::pair<bs::Address, std::string>> result;
   const auto wallet = walletsMgr_->getWalletById(walletId);
   if (wallet == nullptr)
   {
      cb(result);
      return;
   }

   auto&& newAddrVec = wallet->extendAddressChain(count, extInt);
   for (auto& addr : newAddrVec)
   {
      auto&& index = wallet->getAddressIndex(addr);
      auto addrPair = std::make_pair(addr, index);
      result.emplace_back(addrPair);
   }

   cb(result);
}

void InprocSigner::syncAddressBatch(
   const std::string &walletId, const std::set<BinaryData>& addrSet,
   std::function<void(bs::sync::SyncState)> cb)
{
   //grab wallet
   const auto wallet = walletsMgr_->getWalletById(walletId);
   if (wallet == nullptr)
   {
      cb(bs::sync::SyncState::NothingToDo);
      return;
   }

   //resolve the path and address type for addrSet
   std::map<BinaryData, std::pair<bs::hd::Path, AddressEntryType>> parsedMap;
   try
   {
      parsedMap = std::move(wallet->indexPathAndTypes(addrSet));
   }
   catch (AccountException&)
   {
      //failure to find even one of the addresses means the wallet chain needs 
      //extended further
      cb(bs::sync::SyncState::Failure);
   }

   //order addresses by path
   typedef std::map<bs::hd::Path, std::pair<BinaryData, AddressEntryType>> pathMapping;
   std::map<bs::hd::Path::Elem, pathMapping> mapByPath;

   for (auto& parsedPair : parsedMap)
   {
      auto elem = parsedPair.second.first.get(-2);
      auto& mapping = mapByPath[elem];

      auto addrPair = std::make_pair(parsedPair.first, parsedPair.second.second);
      mapping.insert(std::make_pair(parsedPair.second.first, addrPair));
   }

   //strip out addresses using the wallet's default type
   for (auto& mapping : mapByPath)
   {
      auto& addrMap = mapping.second;
      auto iter = addrMap.begin();
      while (iter != addrMap.end())
      {
         if (iter->second.second == AddressEntryType_Default)
         {
            auto eraseIter = iter++;

            /*
            Do not erase this default address if it's the last one in
            the map. The default address type is not a significant piece
            of data to synchronize a wallet's used address chain length,
            however the last instantiated address is relevant, regardless
            of its type
            */

            if (iter != addrMap.end())
               addrMap.erase(eraseIter);

            continue;
         }

         ++iter;
      }
   }

   //request each chain for the relevant address types
   bool update = false;
   for (auto& mapping : mapByPath)
   {
      for (auto& pathPair : mapping.second)
      {
         auto resultPair = wallet->synchronizeUsedAddressChain(
            pathPair.first.toString(), pathPair.second.second);
         update |= resultPair.second;
      }
   }

   if (update)
      cb(bs::sync::SyncState::Success);
   else
      cb(bs::sync::SyncState::NothingToDo);
}

void InprocSigner::setSettlementID(const std::string& wltId
   , const SecureBinaryData &settlId, const std::function<void(bool)> &cb)
{
   /***
   For remote methods, the caller should wait on return before
   proceeding further with settlement flow.
   ***/

   auto leafPtr = walletsMgr_->getWalletById(wltId);
   auto settlLeafPtr = 
      std::dynamic_pointer_cast<bs::core::hd::SettlementLeaf>(leafPtr);
   if (settlLeafPtr == nullptr) {
      if (cb) {
         cb(false);
      }
      return;
   }
   settlLeafPtr->addSettlementID(settlId);

   /*
   Grab the id so that the address is in the asset map. These
   aren't useful to the sync wallet as they never see coins and
   aren't registered.
   */
   settlLeafPtr->getNewExtAddress();
   if (cb) {
      cb(true);
   }
}

void InprocSigner::getSettlementPayinAddress(const std::string& walletID
   , const bs::core::wallet::SettlementData &sd
   , const std::function<void(bool, bs::Address)> &cb)
{
   auto wltPtr = walletsMgr_->getHDWalletById(walletID);
   if (wltPtr == nullptr) {
      if (cb) {
         cb(false, {});
      }
      return;
   }

   if (cb) {
      cb(true, wltPtr->getSettlementPayinAddress(sd));
   }
}

void InprocSigner::getRootPubkey(const std::string& walletID
   , const std::function<void(bool, const SecureBinaryData &)> &cb)
{
   auto leafPtr = walletsMgr_->getWalletById(walletID);
   auto rootPtr = leafPtr->getRootAsset();
   auto rootSingle = std::dynamic_pointer_cast<AssetEntry_Single>(rootPtr);
   if (rootSingle == nullptr) {
      if (cb) {
         cb(false, {});
      }
   }

   if (cb) {
      cb(true, rootSingle->getPubKey()->getCompressedKey());
   }
}
