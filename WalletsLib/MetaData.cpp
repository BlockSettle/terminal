#include "MetaData.h"

#include <QLocale>
#include <QMutexLocker>
#include <bech32/ref/c++/segwit_addr.h>
#include "CheckRecipSigner.h"
#include "CoinSelection.h"
#include "Wallets.h"

#define SAFE_NUM_CONFS        6
#define ASSETMETA_PREFIX      0xAC

std::shared_ptr<bs::wallet::AssetEntryMeta> bs::wallet::AssetEntryMeta::deserialize(int, BinaryDataRef value)
{
   BinaryRefReader brr(value);

   const auto type = brr.get_uint8_t();
   if ((type == bs::wallet::AssetEntryMeta::Comment) && (brr.getSizeRemaining() > 0)) {
      auto aeComment = std::make_shared<bs::wallet::AssetEntryComment>();
      return (aeComment->deserialize(brr) ? aeComment : nullptr);
   }
   throw AssetException("unknown metadata type");
   return nullptr;
}


BinaryData bs::wallet::AssetEntryComment::serialize() const
{
   BinaryWriter bw;
   bw.put_uint8_t(type());

   bw.put_var_int(key_.getSize());
   bw.put_BinaryData(key_);

   bw.put_var_int(comment_.length());
   bw.put_BinaryData(comment_);

   BinaryWriter finalBw;
   finalBw.put_var_int(bw.getSize());
   finalBw.put_BinaryData(bw.getData());
   return finalBw.getData();
}

bool bs::wallet::AssetEntryComment::deserialize(BinaryRefReader brr)
{
   uint64_t len = brr.get_var_int();
   key_ = BinaryData(brr.get_BinaryData(len));

   len = brr.get_var_int();
   comment_ = BinaryData(brr.get_BinaryDataRef(len)).toBinStr();
   return true;
}


void bs::wallet::MetaData::set(const std::shared_ptr<AssetEntryMeta> &value)
{
   data_[value->key()] = value;
}

void bs::wallet::MetaData::write(const std::shared_ptr<LMDBEnv> env, LMDB *db)
{
   if (!env || !db) {
      return;
   }
   for (const auto value : data_) {
      if (!value.second->needsCommit()) {
         continue;
      }
      const auto itData = data_.find(value.first);
      const bool exists = (itData != data_.end());
      auto id = value.second->getIndex();
      if (exists) {
         id = itData->second->getIndex();
      }

      auto&& serializedEntry = value.second->serialize();

      BinaryWriter bw;
      bw.put_uint8_t(ASSETMETA_PREFIX);
      bw.put_uint32_t(id);
      auto &&dbKey = bw.getData();

      CharacterArrayRef keyRef(dbKey.getSize(), dbKey.getPtr());
      CharacterArrayRef dataRef(serializedEntry.getSize(), serializedEntry.getPtr());
      {
         LMDBEnv::Transaction tx(env.get(), LMDB::ReadWrite);
         db->insert(keyRef, dataRef);
      }
      value.second->doNotCommit();
   }
}

void bs::wallet::MetaData::readFromDB(const std::shared_ptr<LMDBEnv> env, LMDB *db)
{
   if ((env == nullptr) || (db == nullptr)) {
      throw WalletException("LMDB is not initialized");
   }
   LMDBEnv::Transaction tx(env.get(), LMDB::ReadOnly);

   auto dbIter = db->begin();

   BinaryWriter bwKey;
   bwKey.put_uint8_t(ASSETMETA_PREFIX);
   CharacterArrayRef keyRef(bwKey.getSize(), bwKey.getData().getPtr());

   dbIter.seek(keyRef, LMDB::Iterator::Seek_GE);

   while (dbIter.isValid()) {
      auto iterkey = dbIter.key();
      auto itervalue = dbIter.value();

      BinaryDataRef keyBDR((uint8_t*)iterkey.mv_data, iterkey.mv_size);
      BinaryDataRef valueBDR((uint8_t*)itervalue.mv_data, itervalue.mv_size);

      BinaryRefReader brrVal(valueBDR);
      auto valsize = brrVal.get_var_int();
      if (valsize != brrVal.getSizeRemaining()) {
         throw WalletException("entry val size mismatch");
      }

      try {
         BinaryRefReader brrKey(keyBDR);

         auto prefix = brrKey.get_uint8_t();
         if (prefix != ASSETMETA_PREFIX) {
            throw AssetException("invalid prefix");
         }
         auto index = brrKey.get_int32_t();
         nbMetaData_ = index + 1;

         auto entryPtr = AssetEntryMeta::deserialize(index, brrVal.get_BinaryDataRef((uint32_t)brrVal.getSizeRemaining()));
         if (entryPtr) {
            entryPtr->doNotCommit();
            data_[entryPtr->key()] = entryPtr;
         }
      }
      catch (AssetException& e) {
         LOGERR << e.what();
         break;
      }

      dbIter.advance();
   }
}


bool bs::wallet::TXSignRequest::isValid() const noexcept
{
   if (!prevStates.empty()) {
      return true;
   }
   if (inputs.empty() || recipients.empty()) {
      return false;
   }
   return true;
}

Signer bs::wallet::TXSignRequest::getSigner() const
{
   bs::CheckRecipSigner signer;

   if (!prevStates.empty()) {
      for (const auto &prevState : prevStates) {
         signer.deserializeState(prevState);
      }
   }
   else {
      signer.setFlags(SCRIPT_VERIFY_SEGWIT);

      for (const auto &utxo : inputs) {
         std::shared_ptr<ScriptSpender> spender;
         if (resolver) {
            spender = std::make_shared<ScriptSpender>(utxo, resolver);
         }
         else if (populateUTXOs) {
            spender = std::make_shared<ScriptSpender>(utxo.getTxHash(), utxo.getTxOutIndex(), utxo.getValue());
         }
         else {
            spender = std::make_shared<ScriptSpender>(utxo);
         }
         if (RBF) {
            spender->setSequence(UINT32_MAX - 2);
         }
         signer.addSpender(spender);
      }
   }
   if (populateUTXOs) {
      for (const auto &utxo : inputs) {
         try {
            signer.populateUtxo(utxo);
         }
         catch (const std::exception &) {}
      }
   }

   for (const auto& recipient : recipients) {
      signer.addRecipient(recipient);
   }

   if (change.value) {
      const auto changeRecip = change.address.getRecipient(change.value);
      if (changeRecip) {
         signer.addRecipient(changeRecip);
      }
   }
   signer.removeDupRecipients();

   if (resolver) {
      signer.setFeed(resolver);
   }
   return signer;
}

// Estimate the TX virtual size. This will not be exact, as there's no sig yet.
// Round up the inputs to guarantee that we meet RBF's relay fee policy.
size_t bs::wallet::estimateTXVirtSize(const std::vector<UTXO> &inputs
              , const std::map<unsigned int, std::shared_ptr<ScriptRecipient>> &recipients)
{
   if (inputs.empty() || recipients.empty()) {
      return 0;
   }

   // Start with 10 bytes in every TX (version, SegWit flags, and lock time).
   size_t result = 10;

   // Get the output virtual sizes from Armory. Assume these will be accurate.
   for (const auto &recip : recipients) {
      result += recip.second->getSize();
   }

   // Estimate the virtual size for the inputs. Because we can't analyze the
   // exact sig size until there's an actual signature, always play it safe and
   // round up the estimated weight by assuming sigs will be at max size.
   // (NB: This function can probably be reworked to be more like TransactionData's
   // UpdateTransactionData() and lean on Armory instead of reinventing the
   // wheel. However, this function isn't used as of Jan. 2019. Don't rewrite
   // until it's used again and can actually be tested. If manually setting
   // values is still required, https://eprint.iacr.org/2018/513.pdf (Table 3)
   // can be consulted for quick-and-dirty values.)
   for (auto& utxo : inputs) {
      const auto scrType = BtcUtils::getTxOutScriptType(utxo.getScript());
      switch (scrType) {
      // 179-181, so assume 181.
      case TXOUT_SCRIPT_STDHASH160:
         result += 181;
         break;
      // 42-44, so assume 44.
      case TXOUT_SCRIPT_P2WPKH:
         result += 44;
         break;
      // Assume P2SH is P2SH-P2WPKH, as we don't spend any other form of P2SH
      // for now. If this changes, Armory doesn't distinguish between P2SH
      // flavors, and another solution will be required. 91-92, so assume 92.
      case TXOUT_SCRIPT_P2SH:
         result += 92;
         break;
      // We should never get here, and if we do, the values will almost
      // certainly be incorrect. Reassess later as needed.
      case TXOUT_SCRIPT_NONSTANDARD:
      default:
         result += 65;
         break;
      }
   }
   return result * 1.4; // rough approximation to Armory's calculations
}

size_t bs::wallet::TXSignRequest::estimateTxVirtSize() const
{
   if (!isValid()) {
      return 0;
   }
   std::map<unsigned int, std::shared_ptr<ScriptRecipient>> recipCopy;
   for (unsigned int i = 0; i < recipients.size(); ++i) {
      recipCopy[i] = recipients[i];
   }
   if (change.value) {
      recipCopy[recipients.size()] = change.address.getRecipient(change.value);
   }
   return bs::wallet::estimateTXVirtSize(inputs, recipCopy);
}


bool bs::wallet::TXMultiSignRequest::isValid() const noexcept
{
   if (inputs.empty() || recipients.empty()) {
      return false;
   }
   return true;
}

size_t bs::wallet::TXMultiSignRequest::estimateTxVirtSize() const
{
   if (!isValid()) {
      return 0;
   }

   std::vector<UTXO> inputsList;
   for (const auto &input : inputs) {
      inputsList.push_back(input.first);
   }
   std::map<unsigned int, std::shared_ptr<ScriptRecipient>> recipCopy;
   for (unsigned int i = 0; i < recipients.size(); ++i) {
      recipCopy[i] = recipients[i];
   }
   return bs::wallet::estimateTXVirtSize(inputsList, recipCopy);
}


bs::wallet::Seed::Seed(const std::string &seed, NetworkType netType)
   : netType_(netType)
{
   try {
      BinaryData base58In(seed);
      base58In.append('\0'); // Remove once base58toScrAddr() is fixed.
      seed_ = BtcUtils::base58toScrAddr(base58In);
   }
   catch (const std::exception &) {
      const auto result = segwit_addr::decode("seed", seed);
      if (result.first >= 0) {
         seed_ = BinaryData(&result.second[0], result.second.size());
      }
      if (seed_.isNull()) {
         seed_ = seed;
      }
   }
}

EasyCoDec::Data bs::wallet::Seed::toEasyCodeChecksum(size_t ckSumSize) const
{
   if (!hasPrivateKey()) {
      return {};
   }
   const size_t halfSize = privKey_.getSize() / 2;
   auto privKeyHalf1 = privKey_.getSliceCopy(0, (uint32_t)halfSize);
   auto privKeyHalf2 = privKey_.getSliceCopy(halfSize, (uint32_t)halfSize);
   const auto hash1 = BtcUtils::getHash256(privKeyHalf1);
   const auto hash2 = BtcUtils::getHash256(privKeyHalf2);
   privKeyHalf1.append(hash1.getSliceCopy(0, (uint32_t)ckSumSize));
   privKeyHalf2.append(hash2.getSliceCopy(0, (uint32_t)ckSumSize));
   const auto chkSumPrivKey = privKeyHalf1 + privKeyHalf2;
   return EasyCoDec().fromHex(chkSumPrivKey.toHexStr());
}

SecureBinaryData bs::wallet::Seed::decodeEasyCodeChecksum(const EasyCoDec::Data &easyData, size_t ckSumSize)
{
   auto const privKeyHalf1 = decodeEasyCodeLineChecksum(easyData.part1, ckSumSize);
   auto const privKeyHalf2 = decodeEasyCodeLineChecksum(easyData.part2, ckSumSize);

   return (privKeyHalf1 + privKeyHalf2);
}

BinaryData bs::wallet::Seed::decodeEasyCodeLineChecksum(
   const std::string& easyCodeHalf, size_t ckSumSize, size_t keyValueSize)
{
    const auto& hexStr = EasyCoDec().toHex(easyCodeHalf);
    const auto keyHalfWithChecksum = BinaryData::CreateFromHex(hexStr);

    size_t halfSize = keyValueSize + ckSumSize;

    if (keyHalfWithChecksum.getSize() != halfSize) {
        throw std::invalid_argument("invalid key size");
    }

    const auto privKeyValue = keyHalfWithChecksum.getSliceCopy(0, (uint32_t)keyValueSize);
    const auto hashValue = keyHalfWithChecksum.getSliceCopy(keyValueSize, (uint32_t)ckSumSize);

    if (BtcUtils::getHash256(privKeyValue).getSliceCopy(0, (uint32_t)ckSumSize) != hashValue) {
        throw std::runtime_error("checksum failure");
    }

    return privKeyValue;
}

bs::wallet::Seed bs::wallet::Seed::fromEasyCodeChecksum(const EasyCoDec::Data &easyData, NetworkType netType
   , size_t ckSumSize)
{
   return bs::wallet::Seed(netType, decodeEasyCodeChecksum(easyData, ckSumSize));
}

bs::wallet::Seed bs::wallet::Seed::fromEasyCodeChecksum(const EasyCoDec::Data &privKey, const EasyCoDec::Data &chainCode
   , NetworkType netType, size_t ckSumSize)
{
   if (chainCode.part1.empty() || chainCode.part2.empty()) {
      return bs::wallet::Seed(netType, decodeEasyCodeChecksum(privKey, ckSumSize));
   }

   return bs::wallet::Seed(netType, decodeEasyCodeChecksum(privKey, ckSumSize)
      , decodeEasyCodeChecksum(chainCode, ckSumSize));
}


bs::Wallet::Wallet(const std::shared_ptr<spdlog::logger> &logger)
   : QObject(nullptr), wallet::MetaData()
   , spendableBalance_(0), unconfirmedBalance_(0), totalBalance_(0)
   , updateAddrBalance_(false), updateAddrTxN_(false), logger_(logger)
{}

bs::Wallet::Wallet()
   : QObject(nullptr), wallet::MetaData()
   , spendableBalance_(0), unconfirmedBalance_(0), totalBalance_(0)
   , updateAddrBalance_(false), updateAddrTxN_(false), logger_(nullptr)
{}

bs::Wallet::~Wallet()
{
   UtxoReservation::delAdapter(utxoAdapter_);
}

std::string bs::Wallet::GetAddressComment(const bs::Address &address) const
{
   const auto aeMeta = get(address.id());
   if ((aeMeta == nullptr) || (aeMeta->type() != bs::wallet::AssetEntryMeta::Comment)) {
      return "";
   }
   const auto aeComment = std::dynamic_pointer_cast<bs::wallet::AssetEntryComment>(aeMeta);
   if (aeComment == nullptr) {
      return "";
   }
   return aeComment->comment();
}

bool bs::Wallet::SetAddressComment(const bs::Address &address, const std::string &comment)
{
   if (address.isNull()) {
      return false;
   }
   set(std::make_shared<bs::wallet::AssetEntryComment>(nbMetaData_++, address.id(), comment));
   write(getDBEnv(), getDB());
   emit addressAdded();
   return true;
}

std::string bs::Wallet::GetTransactionComment(const BinaryData &txHash)
{
   const auto aeMeta = get(txHash);
   if ((aeMeta == nullptr) || (aeMeta->type() != bs::wallet::AssetEntryMeta::Comment)) {
      return {};
   }
   const auto aeComment = std::dynamic_pointer_cast<bs::wallet::AssetEntryComment>(aeMeta);
   return aeComment ? aeComment->comment() : std::string{};
}

bool bs::Wallet::SetTransactionComment(const BinaryData &txOrHash, const std::string &comment)
{
   if (txOrHash.isNull()) {
      return false;
   }

   BinaryData txHash;
   if (txOrHash.getSize() == 32) {
      txHash = txOrHash;
   }
   else {   // raw transaction then
      Tx tx(txOrHash);
      if (!tx.isInitialized()) {
         return false;
      }
      txHash = tx.getThisHash();
   }
   set(std::make_shared<bs::wallet::AssetEntryComment>(nbMetaData_++, txHash, comment));
   write(getDBEnv(), getDB());
   return true;
}

bool bs::Wallet::isBalanceAvailable() const
{
   return (armory_ != nullptr) && (armory_->state() == ArmoryConnection::State::Ready) && (btcWallet_ != nullptr);
}

BTCNumericTypes::balance_type bs::Wallet::GetSpendableBalance() const
{
   if (!isBalanceAvailable()) {
      return -1;
   }
   return spendableBalance_;
}

BTCNumericTypes::balance_type bs::Wallet::GetUnconfirmedBalance() const
{
   if (!isBalanceAvailable()) {
      return 0;
   }
   return unconfirmedBalance_;
}

// Add an unconfirmed delta to the wallet balance. Assume that a negative delta
// indicates spent coins (fees and coins actually sent outside the wallet). A
// positive delta indicates received coins.
void bs::Wallet::AddUnconfirmedBalance(const BTCNumericTypes::balance_type& delta
                                       , const BTCNumericTypes::balance_type& inFees
                                       , const BTCNumericTypes::balance_type& inChgAmt)
{
   if(delta < 0) {
      spendableBalance_ += (delta + inFees + inChgAmt);
      unconfirmedBalance_ += inChgAmt;
   }
   else {
      spendableBalance_ += delta;
      unconfirmedBalance_ += delta;
   }
   totalBalance_ += delta;
}

BTCNumericTypes::balance_type bs::Wallet::GetTotalBalance() const
{
   if (!isBalanceAvailable()) {
      return -1;
   }
   return totalBalance_;
}

template <typename MapT> void bs::Wallet::updateMap(const MapT &src, MapT &dst) const
{
   QMutexLocker lock(&addrMapsMtx_);
   for (const auto &elem : src) {     // std::map::insert doesn't replace elements
      dst[elem.first] = std::move(elem.second);
   }
}

template <typename ArgT> void bs::Wallet::invokeCb(const std::map<BinaryData, ArgT> &data
   , std::map<bs::Address, std::vector<std::function<void(ArgT)>>> &cbMap, const ArgT &defVal) const
{
   for (const auto &queuedCb : cbMap) {
      const auto &it = data.find(queuedCb.first.id());
      if (it != data.end()) {
         for (const auto &cb : queuedCb.second) {
            cb(it->second);
         }
      } else {
         for (const auto &cb : queuedCb.second) {
            cb(defVal);
         }
      }
   }
   cbMap.clear();
}

bool bs::Wallet::getAddrBalance(const bs::Address &addr, std::function<void(std::vector<uint64_t>)> cb) const
{
   if (!isBalanceAvailable()) {
      return false;
   }
   static const std::vector<uint64_t> defVal = { 0, 0, 0 };

   if (updateAddrBalance_) {
      const auto &cbAddrBalance = [this]
         (ReturnMessage<std::map<BinaryData, std::vector<uint64_t>>> balanceMap) {
         try {
            const auto bm = balanceMap.get();
            updateMap<std::map<BinaryData, std::vector<uint64_t>>>(bm, addressBalanceMap_);
            updateAddrBalance_ = false;
         }
         catch(std::exception& e) {
            if(logger_ != nullptr) {
               logger_->error("[getAddrBalance (cbAddrBalance)] Return data " \
                              "error - {}", e.what());
            }
         }

         invokeCb<std::vector<uint64_t>>(addressBalanceMap_, cbBal_, defVal);
      };

      cbBal_[addr].push_back(cb);
      if (cbBal_.size() == 1) {
         btcWallet_->getAddrBalancesFromDB(cbAddrBalance);
      }
   }
   else {
      const auto itBal = addressBalanceMap_.find(addr.id());
      if (itBal == addressBalanceMap_.end()) {
         cb(defVal);
         return true;
      }
      cb(itBal->second);
   }
   return true;
}

bool bs::Wallet::getAddrBalance(const bs::Address &addr) const
{
   const auto &cb = [this, addr](std::vector<uint64_t> balances) {
      emit addrBalanceReceived(addr, balances);
   };
   return getAddrBalance(addr, cb);
}

bool bs::Wallet::getAddrTxN(const bs::Address &addr, std::function<void(uint32_t)> cb) const
{
   if (!isBalanceAvailable()) {
      return false;
   }
   if (updateAddrTxN_) {
      const auto &cbTxN = [this, addr]
                        (ReturnMessage<std::map<BinaryData, uint32_t>> txnMap) {
         try {
            const auto inTxnMap = txnMap.get();
            updateMap<std::map<BinaryData, uint32_t>>(inTxnMap, addressTxNMap_);
            updateAddrTxN_ = false;
         }
         catch (const std::exception &e) {
            if (logger_ != nullptr) {
               logger_->error("[bs::Wallet::getAddrTxN] Return data error - {} ", \
                  "- Address {}", e.what(), addr.display().toStdString());
            }
         }

         invokeCb<uint32_t>(addressTxNMap_, cbTxN_, 0);
      };

      cbTxN_[addr].push_back(cb);
      if (cbTxN_.size() == 1) {
         btcWallet_->getAddrTxnCountsFromDB(cbTxN);
      }
   }
   else {
      const auto itTxN = addressTxNMap_.find(addr.id());
      if (itTxN == addressTxNMap_.end()) {
         cb(0);
         return true;
      }
      cb(itTxN->second);
   }
   return true;
}

bool bs::Wallet::getAddrTxN(const bs::Address &addr) const
{
   const auto &cb = [this, addr](uint32_t txn) {
      emit addrTxNReceived(addr, txn);
   };
   return getAddrTxN(addr, cb);
}

bool bs::Wallet::GetActiveAddressCount(const std::function<void(size_t)> &cb) const
{
   if (!isBalanceAvailable()) {
      return false;
   }
   if (addressTxNMap_.empty() || updateAddrTxN_) {
      const auto &cbTxN = [this, cb] (ReturnMessage<std::map<BinaryData, uint32_t>> txnMap) {
         try {
            auto inTxnMap = txnMap.get();
            updateMap<std::map<BinaryData, uint32_t>>(inTxnMap, addressTxNMap_);
            updateAddrTxN_ = false;
            cb(addressTxNMap_.size());
         } catch (std::exception& e) {
            if (logger_ != nullptr) {
               logger_->error("[bs::Wallet::GetActiveAddressCount] Return data error - {} ", e.what());
            }
         }
      };
      btcWallet_->getAddrTxnCountsFromDB(cbTxN);
   } else {
      cb(addressTxNMap_.size());
   }
   return true;
}

bool bs::Wallet::getSpendableTxOutList(const std::shared_ptr<AsyncClient::BtcWallet> &btcWallet
   , std::function<void(std::vector<UTXO>)> cb, QObject *obj, uint64_t val)
{
   if (!isBalanceAvailable()) {
      return false;
   }

   auto &callbacks = spendableCallbacks_[btcWallet->walletID()];
   callbacks.push_back({ obj, cb });
   if (callbacks.size() > 1) {
      return true;
   }

   const auto &cbTxOutList = [this, val, btcWallet]
                             (ReturnMessage<std::vector<UTXO>> txOutList) {
      try {
         // Before invoking the callbacks, process the UTXOs for the purposes of
         // handling internal/external addresses (UTXO filtering, balance
         // adjusting, etc.).
         auto txOutListObj = txOutList.get();
         const auto &cbProcess = [this, val, btcWallet, txOutListObj] {
            std::vector<UTXO> txOutListCopy = txOutListObj;
            if (utxoAdapter_) {
               utxoAdapter_->filter(txOutListCopy);
            }
            if (val != UINT64_MAX) {
               uint64_t sum = 0;
               int cutOffIdx = -1;
               for (size_t i = 0; i < txOutListCopy.size(); i++) {
                  const auto &utxo = txOutListCopy[i];
                  sum += utxo.getValue();
                  if (sum >= val) {
                     cutOffIdx = (int)i;
                     break;
                  }
               }
               if (cutOffIdx >= 0) {
                  txOutListCopy.resize(cutOffIdx + 1);
               }
            }
            QMetaObject::invokeMethod(this, [this, btcWallet, txOutListCopy] {
               auto &callbacks = spendableCallbacks_[btcWallet->walletID()];
               for (const auto &cbPairs : callbacks) {
                  if (cbPairs.first) {
                        cbPairs.second(txOutListCopy);
                  }
               }
               spendableCallbacks_.erase(btcWallet->walletID());
            });
         };

         cbProcess();
      }
      catch (const std::exception &e) {
         if (logger_ != nullptr) {
            logger_->error("[bs::Wallet::getSpendableTxOutList] Return data " \
               "error {} - value {}", e.what(), val);
         }
      }
   };
   btcWallet->getSpendableTxOutListForValue(val, cbTxOutList);
   return true;
}

bool bs::Wallet::getSpendableTxOutList(std::function<void(std::vector<UTXO>)> cb
   , QObject *obj, uint64_t val)
{
   return getSpendableTxOutList(btcWallet_, cb, obj, val);
}

bool bs::Wallet::getUTXOsToSpend(const std::shared_ptr<AsyncClient::BtcWallet> &btcWallet
   , uint64_t val, std::function<void(std::vector<UTXO>)> cb) const
{
   if (!isBalanceAvailable()) {
      return false;
   }
   const auto &cbProcess = [this, val, cb]
                           (ReturnMessage<std::vector<UTXO>> utxos)-> void {
      try {
         auto utxosObj = utxos.get();
         if (utxoAdapter_) {
            utxoAdapter_->filter(utxosObj);
         }
         std::sort(utxosObj.begin(), utxosObj.end(), [](const UTXO &a, const UTXO &b) {
            return (a.getValue() < b.getValue());
         });

         int index = (int)utxosObj.size() - 1;
         while (index >= 0) {
            if (utxosObj[index].getValue() < val) {
               index++;
               break;
            }
            index--;
         }
         if ((index >= 0) && ((size_t)index < utxosObj.size())) {
            cb({ utxosObj[index] });
            return;
         }
         else if (index < 0) {
            cb({ utxosObj.front() });
            return;
         }

         std::vector<UTXO> result;
         uint64_t sum = 0;
         index = (int)utxosObj.size() - 1;
         while ((index >= 0) && (sum < val)) {  //TODO: needs to be optimized to fill the val more precisely
            result.push_back(utxosObj[index]);
            sum += utxosObj[index].getValue();
            index--;
         }

         if (sum < val) {
            cb({});
         }
         else {
            cb(result);
         }
      }
      catch (const std::exception &e) {
         if (logger_ != nullptr) {
            logger_->error("[bs::Wallet::getUTXOsToSpend] Return data error " \
               "- {} - value {}", e.what(), val);
         }
      }
   };
   btcWallet->getSpendableTxOutListForValue(val, cbProcess);
   return true;
}

bool bs::Wallet::getUTXOsToSpend(uint64_t val, std::function<void(std::vector<UTXO>)> cb) const
{
   return getUTXOsToSpend(btcWallet_, val, cb);
}

bool bs::Wallet::getSpendableZCList(const std::shared_ptr<AsyncClient::BtcWallet> &btcWallet
   , std::function<void(std::vector<UTXO>)> cb, QObject *obj)
{
   if (!isBalanceAvailable()) {
      return false;
   }

   zcListCallbacks_[obj].push_back(cb);
   if (zcListCallbacks_.size() > 1) {
      return true;
   }
   const auto &cbZCList = [this](ReturnMessage<std::vector<UTXO>> utxos)-> void {
      try {
         auto inUTXOs = utxos.get();
         // Before invoking the callbacks, process the UTXOs for the purposes of
         // handling internal/external addresses (UTXO filtering, balance
         // adjusting, etc.).
         const auto &cbProcess = [this, inUTXOs] {
            QMetaObject::invokeMethod(this, [this, inUTXOs] {
               for (const auto &cbPairs : zcListCallbacks_) {
                  if (cbPairs.first) {
                     for (const auto &cb : cbPairs.second) {
                        cb(inUTXOs);
                     }
                  }
               }
               zcListCallbacks_.clear();
            });
         };

         cbProcess();
      }
      catch (const std::exception &e) {
         if (logger_ != nullptr) {
            logger_->error("[bs::Wallet::getSpendableZCList] Return data error " \
               "- {}", e.what());
         }
      }
   };
   btcWallet->getSpendableZCList(cbZCList);
   return true;
}

bool bs::Wallet::getSpendableZCList(std::function<void(std::vector<UTXO>)> cb
   , QObject *obj)
{
   return getSpendableZCList(btcWallet_, cb, obj);
}

bool bs::Wallet::getRBFTxOutList(const std::shared_ptr<AsyncClient::BtcWallet> &btcWallet
   , std::function<void(std::vector<UTXO>)> cb) const
{
   if (!isBalanceAvailable()) {
      return false;
   }

   // The callback we passed in needs data from Armory. Write a simple callback
   // that takes Armory's data and uses it in the callback.
   const auto &cbArmory = [this, cb](ReturnMessage<std::vector<UTXO>> utxos)->void {
      try {
         auto inUTXOs = utxos.get();
         cb(std::move(inUTXOs));
      }
      catch(std::exception& e) {
         if(logger_ != nullptr) {
            logger_->error("[bs::Wallet::getRBFTxOutList] Return data error - " \
               "{}", e.what());
         }
      }
   };

   btcWallet->getRBFTxOutList(cbArmory);
   return true;
}

bool bs::Wallet::getRBFTxOutList(std::function<void(std::vector<UTXO>)> cb) const
{
   return getRBFTxOutList(btcWallet_, cb);
}

// Public frontend for updating a wallet's balances. Required in part because
// Armory doesn't declare TXs safe until 6 confs have occurred.
void bs::Wallet::UpdateBalances(const std::function<void(std::vector<uint64_t>)> &cb)
{
   if (!isBalanceAvailable()) {
      return;
   }
   const auto &cbBalances = [this, cb]
                    (ReturnMessage<std::vector<uint64_t>> balanceVector)->void {
      try {
         auto bv = balanceVector.get();
         if (bv.size() < 4) {
            return;
         }
         const auto totalBalance =
            static_cast<BTCNumericTypes::balance_type>(bv[0]) / BTCNumericTypes::BalanceDivider;
         const auto spendableBalance =
            static_cast<BTCNumericTypes::balance_type>(bv[1]) / BTCNumericTypes::BalanceDivider;
         const auto unconfirmedBalance =
            static_cast<BTCNumericTypes::balance_type>(bv[2]) / BTCNumericTypes::BalanceDivider;
         const auto count = bv[3];

         if ((addrCount_ != count) || (totalBalance_ != totalBalance) || (spendableBalance_ != spendableBalance)
            || (unconfirmedBalance_ != unconfirmedBalance)) {
            {
               QMutexLocker lock(&addrMapsMtx_);
               updateAddrBalance_ = true;
               updateAddrTxN_ = true;
               addrCount_ = count;
            }
            totalBalance_ = totalBalance;
            spendableBalance_ = spendableBalance;
            unconfirmedBalance_ = unconfirmedBalance;

            emit balanceChanged(GetWalletId(), bv);
         }
         emit balanceUpdated(GetWalletId(), bv);

         if (cb) {
            cb(bv);
         }
      }
      catch (const std::exception &e) {
         if (logger_ != nullptr) {
            logger_->error("[bs::Wallet::UpdateBalances] Return data error " \
               "- {}", e.what());
         }
      }
   };
   btcWallet_->getBalancesAndCount(armory_->topBlock(), cbBalances);
}

bool bs::Wallet::getHistoryPage(uint32_t id) const
{
   const auto &cb = [this, id](const bs::Wallet *wallet
      , std::vector<ClientClasses::LedgerEntry> entries) {
      emit historyPageReceived(id, entries);
   };
   return getHistoryPage(id, cb);
}

bool bs::Wallet::getHistoryPage(const std::shared_ptr<AsyncClient::BtcWallet> &btcWallet
   , uint32_t id, std::function<void(const bs::Wallet *wallet
   , std::vector<ClientClasses::LedgerEntry>)> clientCb, bool onlyNew) const
{
   if (!isBalanceAvailable()) {
      return false;
   }
   const auto &cb = [this, id, onlyNew, clientCb]
                    (ReturnMessage<std::vector<ClientClasses::LedgerEntry>> entries)->void {
      try {
         auto le = entries.get();
         if (!onlyNew) {
            clientCb(this, le);
         }
         else {
            const auto &histPage = historyCache_.find(id);
            if (histPage == historyCache_.end()) {
               clientCb(this, le);
            }
            else if (histPage->second.size() == le.size()) {
               clientCb(this, {});
            }
            else {
               std::vector<ClientClasses::LedgerEntry> diff;
               struct comparator {
                  bool operator() (const ClientClasses::LedgerEntry &a, const ClientClasses::LedgerEntry &b) const {
                     return (a.getTxHash() < b.getTxHash());
                  }
               };
               std::set<ClientClasses::LedgerEntry, comparator> diffSet;
               diffSet.insert(le.begin(), le.end());
               for (const auto &entry : histPage->second) {
                  diffSet.erase(entry);
               }
               for (const auto &diffEntry : diffSet) {
                  diff.emplace_back(diffEntry);
               }
               clientCb(this, diff);
            }
         }
         historyCache_[id] = le;
      }
      catch (const std::exception& e) {
         if (logger_ != nullptr) {
            logger_->error("[bs::Wallet::getHistoryPage] Return data " \
               "error - {} - ID {}", e.what(), id);
         }
      }
   };
   btcWallet->getHistoryPage(id, cb);
   return true;
}

bool bs::Wallet::getHistoryPage(uint32_t id, std::function<void(const bs::Wallet *wallet
   , std::vector<ClientClasses::LedgerEntry>)> clientCb, bool onlyNew) const
{
   return getHistoryPage(btcWallet_, id, clientCb, onlyNew);
}

bs::Address bs::Wallet::GetRandomChangeAddress(AddressEntryType aet)
{
   if (GetUsedAddressCount() < 3) {
      return GetNewChangeAddress(aet);
   }
   const auto &addresses = GetUsedAddressList();
   return addresses[qrand() % addresses.size()];
}

bool bs::Wallet::IsSegWitInput(const UTXO& input)
{
   return input.isSegWit() || isSegWitScript(input.getScript());
}

bool bs::Wallet::isSegWitScript(const BinaryData &script)
{
//   const auto scrType = BtcUtils::getTxOutScriptType(script);
   switch (getAddrTypeForAddr(BtcUtils::getTxOutRecipientAddr(script))) {
      case AddressEntryType_P2WPKH:
      case AddressEntryType_P2WSH:
      case AddressEntryType_P2SH:
         return true;
      case AddressEntryType_Default:   // fallback for script not from our wallet
      default: break;                  // fallback for incorrectly deserialized wallet
   }
   return false;
}

QString bs::Wallet::displayTxValue(int64_t val) const
{
   return QLocale().toString(val / BTCNumericTypes::BalanceDivider, 'f', BTCNumericTypes::default_precision);
}

void bs::Wallet::SetArmory(const std::shared_ptr<ArmoryConnection> &armory)
{
   if (!armory_ && (armory != nullptr)) {
      armory_ = armory;
   }
}

std::vector<std::string> bs::Wallet::RegisterWallet(const std::shared_ptr<ArmoryConnection> &armory, bool asNew)
{
   SetArmory(armory);

   if (!utxoAdapter_) {
      utxoAdapter_ = std::make_shared<UtxoFilterAdapter>(GetWalletId());
      if (!UtxoReservation::addAdapter(utxoAdapter_)) {
         utxoAdapter_ = nullptr;
      }
   }

   if (armory_) {
      const auto &cbRegister = [this](const std::string &) {
         emit walletReady(QString::fromStdString(GetWalletId()));
      };
      const auto regId = armory_->registerWallet(btcWallet_, GetWalletId(), getAddrHashes(), cbRegister, asNew);
      return { regId };
   }
   return {};
}

void bs::Wallet::UnregisterWallet()
{
   heartbeatRunning_ = false;
   btcWallet_.reset();
   {
      QMutexLocker lock(&addrMapsMtx_);
      cbBal_.clear();
      cbTxN_.clear();
   }
   spendableCallbacks_.clear();
   zcListCallbacks_.clear();
   historyCache_.clear();

   RegisterWallet();
   btcWallet_.reset();
}

bs::wallet::TXSignRequest bs::Wallet::CreateTXRequest(const std::vector<UTXO> &inputs
   , const std::vector<std::shared_ptr<ScriptRecipient>> &recipients, const uint64_t fee
   , bool isRBF, bs::Address changeAddress, const uint64_t& origFee)
{
   bs::wallet::TXSignRequest request;
   request.walletId = GetWalletId();
   request.wallet = this;
   request.resolver = GetPublicKeyResolver();

   uint64_t inputAmount = 0;
   uint64_t spendAmount = 0;

   if (inputs.empty()) {
      throw std::logic_error("no UTXOs");
   }

   for (const auto& utxo : inputs) {
      inputAmount += utxo.getValue();
   }
   request.inputs = inputs;

   for (const auto& recipient : recipients) {
      if (recipient == nullptr) {
         throw std::logic_error("invalid recipient");
      }
      spendAmount += recipient->getValue();
   }
   if (inputAmount < spendAmount + fee) {
      throw std::logic_error("input amount " + std::to_string(inputAmount) + " is less than spend + fee (" + std::to_string(spendAmount + fee) + ")");
   }

   request.recipients = recipients;
   request.RBF = isRBF;

   // Make sure the incremental relay fee is respected. It's assumed to be 1000
   // sat/KB (1 sat/b). If the user changes this in Core, the bump could fail.
   uint64_t minIncRelayFee = origFee + request.estimateTxVirtSize();
   if(isRBF && fee < minIncRelayFee) {
      request.fee = minIncRelayFee;
   }
   else {
      request.fee = fee;
   }

   const uint64_t changeAmount = inputAmount - (spendAmount + fee);
   if (changeAmount) {
      if (changeAddress.isNull() && (changeAmount >= fee)) {
         changeAddress = GetNewChangeAddress();
         SetAddressComment(changeAddress, bs::wallet::Comment::toString(bs::wallet::Comment::ChangeAddress));
      }
      if (!changeAddress.isNull()) {
         request.change.value = changeAmount;
         request.change.address = changeAddress;
         request.change.index = GetAddressIndex(changeAddress);
      }
   }

   return request;
}

void bs::Wallet::firstInit(bool force)
{
   UpdateBalances();
}

Signer bs::Wallet::getSigner(const wallet::TXSignRequest &request, const SecureBinaryData &password,
                             bool keepDuplicatedRecipients)
{
   bs::CheckRecipSigner signer;
   signer.setFlags(SCRIPT_VERIFY_SEGWIT);

   if (!request.prevStates.empty()) {
      for (const auto &prevState : request.prevStates) {
         signer.deserializeState(prevState);
      }
   }

   if (request.populateUTXOs) {
      for (const auto &utxo : request.inputs) {
         try {
            signer.populateUtxo(utxo);
         }
         catch (const std::exception &) { }
      }
   }
   else {
      for (const auto &utxo : request.inputs) {
         auto spender = std::make_shared<ScriptSpender>(utxo, GetResolver(password));
         if (request.RBF) {
            spender->setSequence(UINT32_MAX - 2);
         }
         signer.addSpender(spender);
      }
   }

   for (const auto &recipient : request.recipients) {
      signer.addRecipient(recipient);
   }

   if (request.change.value && !request.populateUTXOs) {
      auto changeAddress = CreateAddressWithIndex(request.change.index);
      if (changeAddress.isNull() || (changeAddress.prefixed() != request.change.address.prefixed())) {
         changeAddress = request.change.address;   // pub and priv wallets are not synchronized for some reason
      }
      SetAddressComment(changeAddress, bs::wallet::Comment::toString(bs::wallet::Comment::ChangeAddress));
      const auto addr = getAddressEntryForAddr(changeAddress);
      const auto changeRecip = (addr != nullptr) ? addr->getRecipient(request.change.value)
         : changeAddress.getRecipient(request.change.value);
      if (changeRecip == nullptr) {
         throw std::logic_error("invalid change recipient");
      }
      signer.addRecipient(changeRecip);
   }

   if (!keepDuplicatedRecipients) {
      signer.removeDupRecipients();
   }

   signer.setFeed(GetResolver(password));
   return signer;
}

BinaryData bs::Wallet::SignTXRequest(const wallet::TXSignRequest &request,
   const SecureBinaryData &password, bool keepDuplicatedRecipients)
{
   auto signer = getSigner(request, password, keepDuplicatedRecipients);
   signer.sign();
   if (!signer.verify()) {
      throw std::logic_error("signer failed to verify");
   }
   return signer.serialize();
}

BinaryData bs::Wallet::SignPartialTXRequest(const wallet::TXSignRequest &request, const SecureBinaryData &password)
{
   auto signer = getSigner(request, password);
   signer.sign();
   return signer.serializeState();
}

bs::wallet::TXSignRequest bs::Wallet::CreatePartialTXRequest(uint64_t spendVal
   , const std::vector<UTXO> &inputs, bs::Address changeAddress
   , float feePerByte
   , const std::vector<std::shared_ptr<ScriptRecipient>> &recipients
   , const BinaryData prevPart)
{
   uint64_t inputAmount = 0;
   uint64_t fee = 0;
   auto utxos = inputs;
   if (utxos.empty()) {
      throw std::invalid_argument("No usable UTXOs");
   }

   if (feePerByte > 0) {
      unsigned int idMap = 0;
      std::map<unsigned int, std::shared_ptr<ScriptRecipient>> recipMap;
      for (const auto &recip : recipients) {
         if (recip->getValue()) {
            recipMap.emplace(idMap++, recip);
         }
      }

      PaymentStruct payment(recipMap, 0, feePerByte, ADJUST_FEE);
      for (auto &utxo : utxos) {
         const auto addrEntry = getAddressEntryForAddr(utxo.getRecipientScrAddr());
         if (addrEntry) {
            utxo.txinRedeemSizeBytes_ = (unsigned int)addrEntry->getInputSize();
            inputAmount += utxo.getValue();
         }
      }

      if (!inputAmount) {
         throw std::invalid_argument("Couldn't find address entries for UTXOs");
      }

      const auto coinSelection = std::make_shared<CoinSelection>([utxos](uint64_t) { return utxos; }
         , std::vector<AddressBookEntry>{}, GetSpendableBalance() * BTCNumericTypes::BalanceDivider
         , armory_ ? armory_->topBlock() : UINT32_MAX);

      try {
         const auto selection = coinSelection->getUtxoSelectionForRecipients(payment, utxos);
         fee = selection.fee_;
         utxos = selection.utxoVec_;
         inputAmount = selection.value_;
      }
      catch (...) {}
   }
   else {
      size_t nbUtxos = 0;
      for (auto &utxo : utxos) {
         inputAmount += utxo.getValue();
         nbUtxos++;
         if (inputAmount >= (spendVal + fee)) {
            break;
         }
      }
      if (nbUtxos < utxos.size()) {
         utxos.erase(utxos.begin() + nbUtxos, utxos.end());
      }
   }

   if (utxos.empty()) {
      throw std::logic_error("No UTXOs");
   }

   bs::wallet::TXSignRequest request;
   request.walletId = GetWalletId();
   request.wallet = this;
   request.populateUTXOs = true;
   Signer signer;
   if (!prevPart.isNull()) {
      signer.deserializeState(prevPart);
      if (feePerByte > 0) {
         bs::CheckRecipSigner chkSigner(prevPart);
         fee += chkSigner.estimateFee(feePerByte);
      }
   }
   signer.setFlags(SCRIPT_VERIFY_SEGWIT);
   request.fee = fee;

   inputAmount = 0;
   for (const auto& utxo : utxos) {
      signer.addSpender(std::make_shared<ScriptSpender>(utxo.getTxHash(), utxo.getTxOutIndex(), utxo.getValue()));
      request.inputs.push_back(utxo);
      inputAmount += utxo.getValue();
      if (inputAmount >= (spendVal + fee)) {
         break;
      }
   }
   if (!inputAmount) {
      throw std::logic_error("No inputs detected");
   }

   if (!recipients.empty()) {
      uint64_t spendAmount = 0;
      for (const auto& recipient : recipients) {
         if (recipient == nullptr) {
            throw std::logic_error("Invalid recipient");
         }
         spendAmount += recipient->getValue();
         signer.addRecipient(recipient);
      }
      if (spendAmount != spendVal) {
         throw std::invalid_argument("Recipient[s] amount != spend value");
      }
   }
   request.recipients = recipients;

   if (inputAmount > (spendVal + fee)) {
      const uint64_t changeVal = inputAmount - (spendVal + fee);
      if (changeAddress.isNull()) {
         throw std::invalid_argument("Change address required, but missing");
      }
      signer.addRecipient(changeAddress.getRecipient(changeVal));
      request.change.value = changeVal;
      request.change.address = changeAddress;
      request.change.index = GetAddressIndex(changeAddress);
   }

   request.prevStates.emplace_back(signer.serializeState());
   return request;
}

bool bs::Wallet::getLedgerDelegateForAddress(const bs::Address &addr
   , const std::function<void(const std::shared_ptr<AsyncClient::LedgerDelegate> &)> &cb
   , QObject *context)
{
   if (armory_) {
      return armory_->getLedgerDelegateForAddress(GetWalletId(), addr, cb, context);
   }
   return false;
}


BinaryData bs::SignMultiInputTX(const bs::wallet::TXMultiSignRequest &txMultiReq, const bs::cbPassForWallet &cb)
{
   Signer signer;
   if (!txMultiReq.prevState.isNull()) {
      signer.deserializeState(txMultiReq.prevState);
   }
   signer.setFlags(SCRIPT_VERIFY_SEGWIT);

   std::vector<std::shared_ptr<bs::Wallet>> wallets = txMultiReq.wallets;
   if (!txMultiReq.inputs.empty()) {
      wallets.clear();
      for (const auto &input : txMultiReq.inputs) {   // can add inputs to signer here, too
         wallets.push_back(input.second);
      }
   }
   for (const auto &wallet : wallets) {
      if (wallet->isWatchingOnly()) {
         throw std::logic_error("Won't sign with watching-only wallet");
      }
      const SecureBinaryData &password = cb(wallet);
      signer.setFeed(wallet->GetResolver(password));
      signer.sign();
      signer.resetFeeds();
   }

   if (!signer.verify()) {
      throw std::logic_error("signer failed to verify");
   }
   return signer.serialize();
}


size_t bs::wallet::getInputScrSize(const std::shared_ptr<AddressEntry> &addrEntry)
{
   if (addrEntry) {
      switch (addrEntry->getType()) {
      case AddressEntryType_P2SH:   return 64;
      default:    return addrEntry->getInputSize();
      }
   }
   return 65;
}

BinaryData bs::wallet::computeID(const BinaryData &input)
{
   auto result = BtcUtils::computeID(input);
   const auto outSz = result.getSize();
   if (result.getPtr()[outSz - 1] == 0) {
      result.resize(outSz - 1);
   }
   return result;
}


bool operator ==(const bs::Wallet &a, const bs::Wallet &b)
{
   return (a.GetWalletId() == b.GetWalletId());
}

bool operator !=(const bs::Wallet &a, const bs::Wallet &b)
{
   return (a.GetWalletId() != b.GetWalletId());
}
