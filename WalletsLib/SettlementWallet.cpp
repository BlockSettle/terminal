#include "SettlementWallet.h"

#include <stdexcept>

#include <QDir>
#include <QThread>

#include "ArmoryConnection.h"
#include "BTCNumericTypes.h"
#include "CoinSelection.h"
#include "FastLock.h"
#include "ScriptRecipient.h"
#include "Signer.h"
#include "SystemFileUtils.h"

#include <spdlog/spdlog.h>

//https://bitcoin.org/en/developer-guide#term-minimum-fee
constexpr uint64_t MinRelayFee = 1000;

std::shared_ptr<bs::SettlementAddressEntry> bs::SettlementAssetEntry::getAddressEntry(const shared_ptr<SettlementAssetEntry> &assetPtr)
{
   std::shared_ptr<SettlementAssetEntry> sae = dynamic_pointer_cast<SettlementAssetEntry>(assetPtr);
   if (sae == nullptr) {
      throw AssetException("Asset entry is not SettlementAssetEntry");
   }
   std::shared_ptr<SettlementAddressEntry> addr;
   {
      switch (assetPtr->addressType()) {
      case AddressEntryType_Default:
      case AddressEntryType_Multisig:
         addr = std::make_shared<SettlementAddressEntry>(sae);
         break;

      case AddressEntryType_P2SH:
         addr = std::make_shared<SettlementAddressEntry_P2SH>(sae);
         break;

      case AddressEntryType_P2WSH:
         addr = std::make_shared<SettlementAddressEntry_P2WSH>(sae);
         break;

      default:
         throw AssetException("Unsupported address entry type");
      }
   }
   return addr;
}

std::pair<bs::Address, std::shared_ptr<bs::GenericAsset>> bs::SettlementAssetEntry::deserialize(BinaryDataRef value)
{
   BinaryRefReader brrVal(value);
   const auto assetType = static_cast<AssetEntryType>(brrVal.get_uint8_t());
   if (assetType == AssetEntryType_Single) {
      return bs::PlainAsset::deserialize(value);
   }
   const auto id = brrVal.get_int32_t();

   uint64_t len = brrVal.get_var_int();
   const auto settlementId = brrVal.get_BinaryData(len);

   len = brrVal.get_var_int();
   const auto buyAuthPubKey = brrVal.get_BinaryData(len);

   len = brrVal.get_var_int();
   const auto sellAuthPubKey = brrVal.get_BinaryData(len);

   if (settlementId.isNull() || buyAuthPubKey.isNull() || sellAuthPubKey.isNull()) {
      throw AssetException("SettlementAssetEntry: invalid data in DB");
   }
   auto asset = std::make_shared<bs::SettlementAssetEntry>(settlementId, buyAuthPubKey, sellAuthPubKey, id);

   if (brrVal.getSizeRemaining() > 0) {
      len = brrVal.get_var_int();
      const auto script = brrVal.get_BinaryData(len);
      if (!script.isNull()) {
         asset->setScript(script);
         asset->doNotCommit();
      }
   }

   if (brrVal.getSizeRemaining() > 0) {
      const auto addrType = brrVal.get_uint32_t();
      asset->addrType_ = static_cast<AddressEntryType>(addrType);
   }

   return { getAddressEntry(asset)->getPrefixedHash(), asset };
}

BinaryData bs::SettlementAssetEntry::serialize() const
{
   BinaryWriter bw;
   bw.put_uint8_t(static_cast<uint8_t>(getType()));
   bw.put_int32_t(id_);

   bw.put_var_int(settlementId_.getSize());
   bw.put_BinaryData(settlementId_);

   bw.put_var_int(buyAuthPubKey_.getSize());
   bw.put_BinaryData(buyAuthPubKey_);

   bw.put_var_int(sellAuthPubKey_.getSize());
   bw.put_BinaryData(sellAuthPubKey_);

   bw.put_var_int(script_.getSize());
   bw.put_BinaryData(script_);

   uint32_t addrType = static_cast<uint32_t>(addrType_);
   bw.put_uint32_t(addrType);

   return bw.getData();
}

const BinaryData &bs::SettlementAssetEntry::script() const
{
   if (script_.isNull()) {
      const BinaryData &buyChainKey = buyChainedPubKey();
      const BinaryData &sellChainKey = sellChainedPubKey();

      BinaryWriter script;
      script.put_uint8_t(OP_1);
      script.put_uint8_t(buyChainKey.getSize());
      script.put_BinaryData(buyChainKey);
      script.put_uint8_t(sellChainKey.getSize());
      script.put_BinaryData(sellChainKey);
      script.put_uint8_t(OP_2);
      script.put_uint8_t(OP_CHECKMULTISIG);

      script_ = script.getData();
   }
   return script_;
}

const BinaryData &bs::SettlementAssetEntry::buyChainedPubKey() const
{
   if (buyChainedPubKey_.isNull()) {
      CryptoECDSA crypto;
      buyChainedPubKey_ = crypto.CompressPoint(crypto.ComputeChainedPublicKey(crypto.UncompressPoint(buyAuthPubKey()), settlementId()));
   }
   return buyChainedPubKey_;
}

const BinaryData &bs::SettlementAssetEntry::sellChainedPubKey() const
{
   if (sellChainedPubKey_.isNull()) {
      CryptoECDSA crypto;
      sellChainedPubKey_ = crypto.CompressPoint(crypto.ComputeChainedPublicKey(crypto.UncompressPoint(sellAuthPubKey()), settlementId()));
   }
   return sellChainedPubKey_;
}

const BinaryData &bs::SettlementAssetEntry::prefixedHash() const
{
   if (hash_.isNull()) {
      hash_.append(BlockDataManagerConfig::getScriptHashPrefix());
      hash_.append(hash());
   }
   return hash_;
}

const BinaryData &bs::SettlementAssetEntry::p2wshScript() const
{
   if (p2wshScript_.isNull()) {
      const auto hash256 = BtcUtils::getSha256(script());
      Recipient_PW2SH recipient(hash256, 0);
      const auto &script = recipient.getSerializedScript();
      p2wshScript_ = script.getSliceCopy(9, script.getSize() - 9);
   }
   return p2wshScript_;
}

const BinaryData &bs::SettlementAssetEntry::p2wsHash() const
{
   if (p2wsHash_.isNull()) {
      p2wsHash_ = BtcUtils::getHash160(p2wshScript());
   }
   return p2wsHash_;
}

const BinaryData &bs::SettlementAssetEntry::prefixedP2SHash() const
{
   if (prefixedP2SHash_.isNull()) {
      prefixedP2SHash_.append(BlockDataManagerConfig::getScriptHashPrefix());
      prefixedP2SHash_.append(p2wsHash());
   }
   return prefixedP2SHash_;
}

const std::vector<BinaryData> &bs::SettlementAssetEntry::supportedAddresses() const
{
   if (supportedAddresses_.empty()) {
      supportedAddresses_ = { script(), p2wshScript() };
   }
   return supportedAddresses_;
}

const std::vector<BinaryData> &bs::SettlementAssetEntry::supportedAddrHashes() const
{
   if (supportedHashes_.empty()) {
      BinaryData p2wshPrefixed;
      p2wshPrefixed.append(uint8_t(SCRIPT_PREFIX_P2WSH));
      p2wshPrefixed.append(BtcUtils::getSha256(script()));
      supportedHashes_ = { prefixedHash(), prefixedP2SHash(), p2wshPrefixed };
   }
   return supportedHashes_;
}


const BinaryData &bs::SettlementAddressEntry_P2WSH::getHash() const
{
   if (hash_.isNull()) {
      hash_ = BtcUtils::getSha256(ae_->script());
   }
   return hash_;
}

const BinaryData &bs::SettlementAddressEntry_P2WSH::getPrefixedHash(void) const
{
   if (prefixedHash_.isNull()) {
      prefixedHash_.append(uint8_t(SCRIPT_PREFIX_P2WSH));
      prefixedHash_.append(getHash());
   }
   return prefixedHash_;
}


class SettlementResolverFeed : public ResolverFeed
{
private:
   template<class payloadType>
   struct FeedItem
   {
      payloadType payload;
      std::string description;
   };

public:
   SettlementResolverFeed(const std::shared_ptr<bs::SettlementAddressEntry> &addr, const bs::KeyPair &keys) {
      CryptoECDSA crypto;
      const auto chainCode = addr->getAsset()->settlementId();
      const auto chainedPrivKey = crypto.ComputeChainedPrivateKey(keys.privKey, chainCode);
      const auto chainedPubKey = crypto.CompressPoint(crypto.ComputeChainedPublicKey(crypto.UncompressPoint(keys.pubKey), chainCode));

      keys_[chainedPubKey] = FeedItem<SecureBinaryData>{ chainedPrivKey, "Private key" };

      values_[BtcUtils::getSha256(addr->getAsset()->script())] = FeedItem<BinaryData>{addr->getAsset()->script(), "Address"};
      values_[addr->getAsset()->hash()] = FeedItem<BinaryData>{addr->getAsset()->script(), "Script"};
      values_[addr->getAsset()->p2wsHash()] = FeedItem<BinaryData>{addr->getAsset()->p2wshScript(), "P2WSHScript"};
   }

   BinaryData getByVal(const BinaryData& val) override {
      auto it = values_.find(val);
      if (it == values_.end()) {
         throw std::runtime_error("Unknown value key");
      }
      return it->second.payload;
   }

   const SecureBinaryData& getPrivKeyForPubkey(const BinaryData& pubkey) override {
      auto it = keys_.find(pubkey);
      if (it == keys_.end()) {
         throw std::runtime_error("Unknown pubkey");
      }
      return it->second.payload;
   }

private:
   std::map<BinaryData, struct FeedItem<BinaryData>> values_;
   std::map<BinaryData, struct FeedItem<SecureBinaryData>> keys_;
};


bs::SettlementWallet::SettlementWallet()
   : PlainWallet(tr("Settlement").toStdString(), tr("Settlement Wallet").toStdString())
{}

bs::SettlementWallet::SettlementWallet(const std::string &filename)
   : PlainWallet()
{
   loadFromFile(filename);
}

std::string bs::SettlementWallet::getFileName(const std::string &dir) const
{
   return dir + "/" + fileNamePrefix() + "wallet.lmdb";
}

int  bs::SettlementWallet::addAddress(const std::shared_ptr<SettlementAddressEntry> &addrEntry
   , const std::shared_ptr<SettlementAssetEntry> &asset)
{
   const int id = addAddress(addrEntry->getPrefixedHash(), asset);

   auto settlementId = asset->settlementId();
   FastLock lock(lockAddressMap_);
   addrEntryByAddr_[addrEntry->getPrefixedHash()] = addrEntry;
   addressBySettlementId_[settlementId] = addrEntry;
   return id;
}

int bs::SettlementWallet::addAddress(const bs::Address &addr, std::shared_ptr<bs::GenericAsset> asset)
{
   const int id = bs::PlainWallet::addAddress(addr, asset);
   const auto settlAsset = dynamic_pointer_cast<SettlementAssetEntry>(asset);
   const auto &addrHashes = settlAsset->supportedAddrHashes();
   addrPrefixedHashes_.insert(addrHashes.begin(), addrHashes.end());
   for (const auto &hash : addrHashes) {
      assetByAddr_[hash] = asset;
   }
   return id;
}

AddressEntryType bs::SettlementWallet::getAddrTypeForAddr(const BinaryData &addr)
{
   BinaryData prefixed;
   prefixed.append(BlockDataManagerConfig::getScriptHashPrefix());
   prefixed.append(addr);
   const auto itAsset = assetByAddr_.find(prefixed);
   if (itAsset != assetByAddr_.end()) {
      const auto settlAsset = dynamic_pointer_cast<SettlementAssetEntry>(itAsset->second);
      if (settlAsset) {
         return settlAsset->addressType();
      }
   }
   return AddressEntryType_Default;
}

std::shared_ptr<bs::SettlementAddressEntry> bs::SettlementWallet::getExistingAddress(const BinaryData &settlementId)
{
   auto address = getAddressBySettlementId(settlementId);
   if (address != nullptr) {
      createTempWalletForAsset(address->getAsset());
   }

   return address;
}

std::shared_ptr<bs::SettlementAddressEntry> bs::SettlementWallet::getAddressBySettlementId(const BinaryData &settlementId) const
{
   FastLock locker(lockAddressMap_);
   auto it = addressBySettlementId_.find(settlementId);
   if (it != addressBySettlementId_.end()) {
      return it->second;
   }

   return nullptr;
}

void bs::SettlementWallet::createTempWalletForAsset(const std::shared_ptr<SettlementAssetEntry>& asset)
{
   auto index = asset->getIndex();
   const auto walletId = BtcUtils::scrAddrToBase58(asset->prefixedHash()).toBinStr();
   armory_->registerWallet(rtWallets_[index], walletId, asset->supportedAddrHashes(), [] {}, true);
   rtWalletsById_[walletId] = index;
}

std::shared_ptr<bs::SettlementAddressEntry> bs::SettlementWallet::newAddress(const BinaryData &settlementId, const BinaryData &buyAuthPubKey
   , const BinaryData &sellAuthPubKey, const std::string &comment)
{
   auto asset = std::make_shared<SettlementAssetEntry>(settlementId, buyAuthPubKey, sellAuthPubKey);
   auto aePtr = SettlementAssetEntry::getAddressEntry(asset);

   int id = addAddress(aePtr, asset);
   writeDB();

   if (!comment.empty()) {
      MetaData::set(std::make_shared<bs::wallet::AssetEntryComment>(id, aePtr->getPrefixedHash(), comment));
      MetaData::write(getDBEnv(), getDB());
   }

   if (armory_) {
      createTempWalletForAsset(asset);
      RegisterWallet(armory_);
   }

   emit addressAdded();
   return aePtr;
}

std::string bs::SettlementWallet::GetAddressIndex(const bs::Address &addr)
{
   const auto assetIt = assetByAddr_.find(addr.id());
   if (assetIt == assetByAddr_.end()) {
      return {};
   }
   const auto asset = dynamic_pointer_cast<SettlementAssetEntry>(assetIt->second);
   if (!asset) {
      return {};
   }
   return asset->settlementId().toHexStr() + "." + asset->buyAuthPubKey().toHexStr()
      + "." + asset->sellAuthPubKey().toHexStr();
}

bool bs::SettlementWallet::AddressIndexExists(const std::string &index) const
{
   const auto pos1 = index.find('.');
   if (pos1 == std::string::npos) {
      return false;
   }
   const auto &binSettlementId = BinaryData::CreateFromHex(index.substr(0, pos1));
   return (getAddressBySettlementId(binSettlementId) != nullptr);
}

bs::Address bs::SettlementWallet::CreateAddressWithIndex(const std::string &index, AddressEntryType aet, bool)
{
   if (index.empty()) {
      return {};
   }
   const auto pos1 = index.find('.');
   if (pos1 == std::string::npos) {
      return {};
   }
   const auto &binSettlementId = BinaryData::CreateFromHex(index.substr(0, pos1));
   const auto addrEntry = getAddressBySettlementId(binSettlementId);
   if (addrEntry) {
      return addrEntry->getPrefixedHash();
   }

   const auto pos2 = index.find_last_of('.');
   if (pos2 == pos1) {
      return {};
   }
   const auto buyAuthKey = index.substr(pos1 + 1, pos2 - pos1);
   const auto sellAuthKey = index.substr(pos2 + 1);
   return newAddress(binSettlementId, BinaryData::CreateFromHex(buyAuthKey)
      , BinaryData::CreateFromHex(sellAuthKey))->getPrefixedHash();
}

bool bs::SettlementWallet::containsAddress(const bs::Address &addr)
{
   if (addrPrefixedHashes_.find(addr.prefixed()) != addrPrefixedHashes_.end()) {
      return true;
   }
   if (addressHashes_.find(addr.unprefixed()) != addressHashes_.end()) {
      return true;
   }
   if (!GetAddressIndex(addr).empty()) {
      return true;
   }
   return false;
}

bool bs::SettlementWallet::hasWalletId(const std::string &id) const
{
   if (id == GetWalletId()) {
      return true;
   }
   if (isTempWalletId(id)) {
      return true;
   }
   return false;
}

bool bs::SettlementWallet::isTempWalletId(const std::string &id) const
{
   return rtWalletsById_.find(id) != rtWalletsById_.end();
}

bool bs::SettlementWallet::GetInputFor(const shared_ptr<SettlementAddressEntry> &addr, std::function<void(UTXO)> cb
   , bool allowZC)
{
   const auto &rtWallet = rtWallets_[addr->getIndex()];
   if (rtWallet == nullptr) {
      return false;
   }

   const auto &cbSpendable = [this, cb, allowZC, rtWallet](std::vector<UTXO> inputs) {
      if (inputs.empty()) {
         if (allowZC) {
            const auto &cbZC = [cb](std::vector<UTXO> zcs) {
               if (zcs.size() == 1) {
                  cb(zcs[0]);
               }
            };
            rtWallet->getSpendableZCList(cbZC);
         }
      }
      else if (inputs.size() == 1) {
         cb(inputs[0]);
      }
   };
   rtWallet->getSpendableTxOutListForValue(UINT64_MAX, cbSpendable);
   return true;
}

uint64_t bs::SettlementWallet::GetEstimatedFeeFor(UTXO input, const bs::Address &recvAddr, float feePerByte)
{
   const auto inputAmount = input.getValue();
   if (input.txinRedeemSizeBytes_ == UINT32_MAX) {
      const auto addrEntry = getAddressEntryForAddr(input.getRecipientScrAddr());
      input.txinRedeemSizeBytes_ = bs::wallet::getInputScrSize(addrEntry);
   }
   CoinSelection coinSelection([&input](uint64_t) -> std::vector<UTXO> { return { input }; }
   , std::vector<AddressBookEntry>{}, inputAmount, armory_->topBlock());

   const auto &scriptRecipient = recvAddr.getRecipient(inputAmount);
   return coinSelection.getFeeForMaxVal(scriptRecipient->getSize(), feePerByte, { input });
}

UTXO bs::SettlementWallet::GetInputFromTX(const shared_ptr<SettlementAddressEntry> &addr, const BinaryData &payinHash, const double amount) const
{
   const uint64_t value = amount * BTCNumericTypes::BalanceDivider;
   const uint32_t txHeight = UINT32_MAX;
   const auto hash = BtcUtils::getSha256(addr->getScript());

   return UTXO(value, txHeight, 0, 0, payinHash, BtcUtils::getP2WSHOutputScript(hash));
}

bs::wallet::TXSignRequest bs::SettlementWallet::CreatePayoutTXRequest(const UTXO &input, const bs::Address &recvAddr
   , float feePerByte)
{
   bs::wallet::TXSignRequest txReq;
   txReq.inputs.push_back(input);
   uint64_t fee = GetEstimatedFeeFor(input, recvAddr, feePerByte);

   if (fee < MinRelayFee) {
      fee = MinRelayFee;
   }

   uint64_t value = input.getValue();
   if (value < fee) {
      value = 0;
   } else {
      value = value - fee;
   }

   txReq.fee = fee;
   txReq.recipients.emplace_back(recvAddr.getRecipient(value));
   return txReq;
}

BinaryData bs::SettlementWallet::SignPayoutTXRequest(const bs::wallet::TXSignRequest &req, const KeyPair &keys
   , const BinaryData &settlementId, const BinaryData &buyAuthKey, const BinaryData &sellAuthKey)
{
   auto addr = getAddressBySettlementId(settlementId);
   if (!addr) {
      addr = newAddress(settlementId, buyAuthKey, sellAuthKey);
   }
   auto resolverFeed = std::make_shared<SettlementResolverFeed>(addr, keys);

   Signer signer;
   signer.setFlags(SCRIPT_VERIFY_SEGWIT);

   if ((req.inputs.size() == 1) && (req.recipients.size() == 1)) {
      auto spender = std::make_shared<ScriptSpender>(req.inputs[0], resolverFeed);
      signer.addSpender(spender);
      signer.addRecipient(req.recipients[0]);
   }
   else if (!req.prevStates.empty()) {
      for (const auto &prevState : req.prevStates) {
         signer.deserializeState(prevState);
      }
   }

   if (req.populateUTXOs) {
      for (const auto &utxo : req.inputs) {
         signer.populateUtxo(utxo);
      }
   }
   signer.setFeed(resolverFeed);

   signer.sign();
   if (!signer.verify()) {
      throw std::logic_error("signer failed to verify");
   }
   return signer.serialize();
}


std::shared_ptr<AddressEntry> bs::SettlementWallet::getAddressEntryForAddr(const BinaryData &addr)
{
   const auto &itAddrEntry = addrEntryByAddr_.find(addr);
   if (itAddrEntry == addrEntryByAddr_.end()) {
      return nullptr;
   }
   return itAddrEntry->second;
}

SecureBinaryData bs::SettlementWallet::GetPublicKeyFor(const bs::Address &addr)
{
   if (addr.isNull()) {
      return {};
   }
   const auto &itAsset = assetByAddr_.find(addr);
   if (itAsset == assetByAddr_.end()) {
      return {};
   }
   const auto settlAsset = dynamic_pointer_cast<SettlementAssetEntry>(itAsset->second);
   return settlAsset ? settlAsset->settlementId() : SecureBinaryData{};
}

bs::KeyPair bs::SettlementWallet::GetKeyPairFor(const bs::Address &addr, const SecureBinaryData &password)
{
   return {};
}

bool bs::SettlementWallet::getSpendableZCList(std::function<void(std::vector<UTXO>)> cb, QObject *obj)
{
   auto result = new std::vector<UTXO>;
   auto walletSet = new std::unordered_set<int>;
   for (const auto &rtWallet : rtWallets_) {
      walletSet->insert(rtWallet.first);
   }
   const auto &cbZCList = [this, result, walletSet, cb](std::vector<UTXO> utxos) {
      result->insert(result->end(), utxos.begin(), utxos.end());

      for (const auto &rtWallet : rtWallets_) {
         const auto &cbRTWlist = [result, walletSet, id=rtWallet.first, cb](std::vector<UTXO> utxos) {
            result->insert(result->end(), utxos.begin(), utxos.end());
            walletSet->erase(id);
            if (walletSet->empty()) {
               delete walletSet;
               cb(*result);
               delete result;
            }
         };
         rtWallet.second->getSpendableZCList(cbRTWlist);
      }
   };
   return bs::Wallet::getSpendableZCList(cbZCList, obj);
}

std::shared_ptr<bs::SettlementMonitor> bs::SettlementWallet::createMonitor(const shared_ptr<bs::SettlementAddressEntry> &addr
   , const std::shared_ptr<spdlog::logger>& logger)
{
   const auto rtWallet = rtWallets_[addr->getIndex()];
   if (rtWallet == nullptr) {
      return nullptr;
   }
   return std::make_shared<bs::SettlementMonitor>(rtWallet, armory_, addr, logger);
}


bs::SettlementMonitor::SettlementMonitor(const std::shared_ptr<AsyncClient::BtcWallet> rtWallet
   , const std::shared_ptr<ArmoryConnection> &armory
   , const shared_ptr<bs::SettlementAddressEntry> &addr
   , const std::shared_ptr<spdlog::logger>& logger
   , QObject *parent)
 : QObject(parent)
 , rtWallet_(rtWallet)
 , armory_(armory)
 , addressEntry_(addr)
 , payinConfirmations_(-1)
 , payoutConfirmations_(-1)
 , payinInBlockChain_(false)
 , payoutConfirmedFlag_(false)
 , logger_(logger)
 , stopped_(false)
{
   const auto &addrHashes = addr->getAsset()->supportedAddrHashes();
   ownAddresses_.insert(addrHashes.begin(), addrHashes.end());
   id_ = addr->getIndex();

   addressString_ = bs::Address{addressEntry_->getPrefixedHash()}.display<std::string>();
}

void bs::SettlementMonitor::start()
{
   connect(armory_.get(), &ArmoryConnection::zeroConfReceived, this
      , &bs::SettlementMonitor::checkNewEntries, Qt::QueuedConnection);
   connect(armory_.get(), &ArmoryConnection::newBlock, this
      , &bs::SettlementMonitor::checkNewEntries, Qt::QueuedConnection);

   checkNewEntries(0);
}

void bs::SettlementMonitor::stop()
{
   stopped_ = true;
   disconnect(armory_.get(), &ArmoryConnection::zeroConfReceived, this
      , &bs::SettlementMonitor::checkNewEntries);
   disconnect(armory_.get(), &ArmoryConnection::newBlock, this
      , &bs::SettlementMonitor::checkNewEntries);
}

void bs::SettlementMonitor::checkNewEntries(unsigned int)
{
   logger_->debug("[SettlementMonitor::checkNewEntries] checking entries for {}"
      , addressString_);

   const auto &cbHistory = [this] (std::vector<ClientClasses::LedgerEntry> entries) {
      if (stopped_ || entries.empty()) {
         return;
      }

      for (const auto &entry : entries) {
         const auto &cbPayOut = [this, entry](bool ack) {
            if (ack) {
               SendPayOutNotification(entry);
            }
            else {
               logger_->error("[SettlementMonitor::checkNewEntries] not payin or payout transaction detected for settlement address {}"
                  , addressString_);
            }
         };
         const auto &cbPayIn = [this, entry, cbPayOut](bool ack) {
            if (ack) {
               SendPayInNotification(armory_->getConfirmationsNumber(entry), entry.getTxHash());
            }
            else {
               IsPayOutTransaction(entry, cbPayOut);
            }
         };
         IsPayInTransaction(entry, cbPayIn);
      }
      {
         FastLock locker(walletLock_);
         if (!rtWallet_) {
            return;
         }
      }
   };
   {
      FastLock locker(walletLock_);
      if (!rtWallet_) {
         return;
      }
   }
   rtWallet_->getHistoryPage(0, cbHistory);  //XXX use only the first page for monitoring purposes
}

void bs::SettlementMonitor::IsPayInTransaction(const ClientClasses::LedgerEntry &entry
   , std::function<void(bool)> cb) const
{
   const auto &cbTX = [this, entry, cb](Tx tx) {
      if (!tx.isInitialized()) {
         logger_->error("[bs::SettlementMonitor::IsPayInTransaction] TX not initialized for {}."
            , entry.getTxHash().toHexStr());
         cb(false);
         return;
      }

      for (int i = 0; i < tx.getNumTxOut(); ++i) {
         TxOut out = tx.getTxOutCopy(i);
         auto address = out.getScrAddressStr();
         if (ownAddresses_.find(address) != ownAddresses_.end()) {
            cb(true);
            return;
         }
      }
      cb(false);
   };
   armory_->getTxByHash(entry.getTxHash(), cbTX);
}

void bs::SettlementMonitor::IsPayOutTransaction(const ClientClasses::LedgerEntry &entry
   , std::function<void(bool)> cb) const
{
   const auto &cbTX = [this, entry, cb](Tx tx) {
      if (!tx.isInitialized()) {
         logger_->error("[bs::SettlementMonitor::IsPayOutTransaction] TX not initialized for {}."
            , entry.getTxHash().toHexStr());
         cb(false);
         return;
      }
      std::set<BinaryData> txHashSet;
      std::map<BinaryData, std::set<uint32_t>> txOutIdx;

      for (int i = 0; i < tx.getNumTxIn(); ++i) {
         TxIn in = tx.getTxInCopy(i);
         OutPoint op = in.getOutPoint();

         txHashSet.insert(op.getTxHash());
         txOutIdx[op.getTxHash()].insert(op.getTxOutIndex());
      }

      const auto &cbTXs = [this, txOutIdx, cb](std::vector<Tx> txs) {
         for (const auto &prevTx : txs) {
            const auto &itIdx = txOutIdx.find(prevTx.getThisHash());
            if (itIdx == txOutIdx.end()) {
               continue;
            }
            for (const auto &txOutI : itIdx->second) {
               const TxOut prevOut = prevTx.getTxOutCopy(txOutI);
               const auto &address = prevOut.getScrAddressStr();
               if (ownAddresses_.find(address) != ownAddresses_.end()) {
                  cb(true);
                  return;
               }
            }
         }
         cb(false);
      };
      armory_->getTXsByHash(txHashSet, cbTXs);
   };
   armory_->getTxByHash(entry.getTxHash(), cbTX);
}

void bs::SettlementMonitor::SendPayInNotification(const int confirmationsNumber, const BinaryData &txHash)
{
   if ((confirmationsNumber != payinConfirmations_) && (!payinInBlockChain_)){

      logger_->debug("[SettlementMonitor::SendPayInNotification] payin detected for {}. Confirmations: {}"
            , addressString_, confirmationsNumber);

      emit payInDetected(confirmationsNumber, txHash);

      payinInBlockChain_ = (confirmationsNumber != 0);
      payinConfirmations_ = confirmationsNumber;
   }
}

void bs::SettlementMonitor::SendPayOutNotification(const ClientClasses::LedgerEntry &entry)
{
   auto confirmationsNumber = armory_->getConfirmationsNumber(entry);
   if (payoutConfirmations_ != confirmationsNumber) {
      payoutConfirmations_ = confirmationsNumber;

      const auto &cbPayoutType = [this](bs::PayoutSigner::Type poType) {
         payoutSignedBy_ = poType;
         if (payoutConfirmations_ >= confirmedThreshold()) {
            if (!payoutConfirmedFlag_) {
               payoutConfirmedFlag_ = true;
               logger_->debug("[SettlementMonitor::SendPayOutNotification] confirmed payout for {}"
                  , addressString_);
               emit payOutConfirmed(payoutSignedBy_);
            }
         }
         else {
            logger_->debug("[SettlementMonitor::SendPayOutNotification] payout for {}. Confirmations: {}"
               , addressString_, payoutConfirmations_);
            emit payOutDetected(payoutConfirmations_, payoutSignedBy_);
         }
      };
      CheckPayoutSignature(entry, cbPayoutType);
   }
}

void bs::PayoutSigner::WhichSignature(const Tx& tx
   , uint64_t value
   , const std::shared_ptr<bs::SettlementAddressEntry> &ae
   , const std::shared_ptr<spdlog::logger> &logger
   , const std::shared_ptr<ArmoryConnection> &armory, std::function<void(Type)> cb)
{
   if (!tx.isInitialized()) {
      cb(SignatureUndefined);
      return;
   }

   struct Result {
      std::set<BinaryData> txHashSet;
      std::map<BinaryData, std::set<uint32_t>>  txOutIdx;
      uint64_t value;
   };
   auto result = new Result;
   result->value = value;

   const auto &cbProcess = [result, ae, tx, cb, logger](std::vector<Tx> txs) {
      for (const auto &prevTx : txs) {
         const auto &txHash = prevTx.getThisHash();
         for (const auto &txOutIdx : result->txOutIdx[txHash]) {
            TxOut prevOut = prevTx.getTxOutCopy(txOutIdx);
            result->value += prevOut.getValue();
         }
         result->txHashSet.erase(txHash);
      }

      uint32_t txHeight = UINT32_MAX;
      uint32_t txIndex = 0, txOutIndex = 0;
      const unsigned inputId = 0;

      const auto settlAddrHash = BtcUtils::getSha256(ae->getScript());
      const TxIn in = tx.getTxInCopy(inputId);
      const OutPoint op = in.getOutPoint();
      const auto payinHash = op.getTxHash();

      UTXO utxo(result->value, txHeight, txIndex, txOutIndex, payinHash, BtcUtils::getP2WSHOutputScript(settlAddrHash));

      //serialize signed tx
      auto txdata = tx.serialize();
      auto bctx = BCTX::parse(txdata);

      map<BinaryData, map<unsigned, UTXO>> utxoMap;

      utxoMap[utxo.getTxHash()][inputId] = utxo;

      //setup verifier
      try {
         TransactionVerifier tsv(*bctx, utxoMap);

         auto tsvFlags = tsv.getFlags();
         tsvFlags |= SCRIPT_VERIFY_P2SH_SHA256 | SCRIPT_VERIFY_P2SH | SCRIPT_VERIFY_SEGWIT;
         tsv.setFlags(tsvFlags);

         auto verifierState = tsv.evaluateState();

         auto inputState = verifierState.getSignedStateForInput(inputId);

         if (inputState.getSigCount() == 0) {
            logger->error("[bs::PayoutSigner::WhichSignature] no signatures received for TX: {}"
               , tx.getThisHash().toHexStr());
         }

         if (inputState.isSignedForPubKey(ae->getAsset()->buyChainedPubKey())) {
            cb(SignedByBuyer);
            return;
         }
         else if (inputState.isSignedForPubKey(ae->getAsset()->sellChainedPubKey())) {
            cb(SignedBySeller);
            return;
         }
      }
      catch (const std::exception &e) {
         logger->error("[PayoutSigner::WhichSignature] exception {}", e.what());
      }
      cb(SignatureUndefined);
   };
   if (value == 0) {    // needs to be a sum of inputs in this case
      for (size_t i = 0; i < tx.getNumTxIn(); ++i) {
         const OutPoint op = tx.getTxInCopy(i).getOutPoint();
         result->txHashSet.insert(op.getTxHash());
         result->txOutIdx[op.getTxHash()].insert(op.getTxOutIndex());
      }
      armory->getTXsByHash(result->txHashSet, cbProcess);
   }
   else {
      cbProcess({});
   }
}

void bs::SettlementMonitor::CheckPayoutSignature(const ClientClasses::LedgerEntry &entry
   , std::function<void(PayoutSigner::Type)> cb) const
{
   const auto amount = entry.getValue();
   const uint64_t value = amount < 0 ? -amount : amount;

   const auto &cbTX = [this, value, cb](Tx tx) {
      bs::PayoutSigner::WhichSignature(tx, value, addressEntry_, logger_, armory_, cb);
   };

   if (!armory_->getTxByHash(entry.getTxHash(), cbTX)) {
      logger_->error("[SettlementMonitor::CheckPayoutSignature] failed to get TX by hash");
   }
}

bs::SettlementMonitor::~SettlementMonitor() noexcept
{
   FastLock locker(walletLock_);
   rtWallet_ = nullptr;
}
