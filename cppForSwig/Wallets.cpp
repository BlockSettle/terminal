////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//  Copyright (C) 2016, goatpig                                               //
//  Distributed under the MIT license                                         //
//  See LICENSE-MIT or https://opensource.org/licenses/MIT                    //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

#include "Wallets.h"
#include "BlockDataManagerConfig.h"
#include "BIP32_Node.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
//// WalletMeta
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
WalletMeta::~WalletMeta()
{}

////////////////////////////////////////////////////////////////////////////////
BinaryData WalletMeta::getDbKey()
{
   if (walletID_.getSize() == 0)
      throw WalletException("empty master ID");

   BinaryWriter bw;
   bw.put_uint8_t(WALLETMETA_PREFIX);
   bw.put_BinaryData(walletID_);

   return bw.getData();
}

////////////////////////////////////////////////////////////////////////////////
BinaryData WalletMeta::serializeVersion() const
{
   BinaryWriter bw;
   bw.put_uint8_t(versionMajor_);
   bw.put_uint16_t(versionMinor_);
   bw.put_uint16_t(revision_);

   return bw.getData();
}

////////////////////////////////////////////////////////////////////////////////
void WalletMeta::unseralizeVersion(BinaryRefReader& brr)
{
   versionMajor_ = brr.get_uint8_t();
   versionMinor_ = brr.get_uint16_t();
   revision_ = brr.get_uint16_t();
}

////////////////////////////////////////////////////////////////////////////////
BinaryData WalletMeta::serializeEncryptionKey() const
{
   BinaryWriter bw;
   bw.put_var_int(defaultEncryptionKeyId_.getSize());
   bw.put_BinaryData(defaultEncryptionKeyId_);
   bw.put_var_int(defaultEncryptionKey_.getSize());
   bw.put_BinaryData(defaultEncryptionKey_);

   return bw.getData();
}

////////////////////////////////////////////////////////////////////////////////
void WalletMeta::unserializeEncryptionKey(BinaryRefReader& brr)
{
   auto len = brr.get_var_int();
   defaultEncryptionKeyId_ = move(brr.get_BinaryData(len));
   
   len = brr.get_var_int();
   defaultEncryptionKey_ = move(brr.get_BinaryData(len));
}

////////////////////////////////////////////////////////////////////////////////
BinaryData WalletMeta_Single::serialize() const
{
   BinaryWriter bw;
   bw.put_uint32_t(type_);
   bw.put_BinaryData(serializeVersion());
   bw.put_BinaryData(serializeEncryptionKey());

   BinaryWriter final_bw;
   final_bw.put_var_int(bw.getSize());
   final_bw.put_BinaryDataRef(bw.getDataRef());

   return final_bw.getData();
}

////////////////////////////////////////////////////////////////////////////////
bool WalletMeta_Single::shouldLoad() const
{
   return true;
}

////////////////////////////////////////////////////////////////////////////////
BinaryData WalletMeta_Multisig::serialize() const
{
   BinaryWriter bw;
   bw.put_uint32_t(type_);
   bw.put_BinaryData(serializeVersion());
   bw.put_BinaryData(serializeEncryptionKey());

   BinaryWriter final_bw;
   final_bw.put_var_int(bw.getSize());
   final_bw.put_BinaryDataRef(bw.getDataRef());

   return final_bw.getData();
}

////////////////////////////////////////////////////////////////////////////////
bool WalletMeta_Multisig::shouldLoad() const
{
   return true;
}

////////////////////////////////////////////////////////////////////////////////
BinaryData WalletMeta_Subwallet::serialize() const
{
   BinaryWriter bw;
   bw.put_var_int(4);
   bw.put_uint32_t(type_);

   return bw.getData();
}

////////////////////////////////////////////////////////////////////////////////
bool WalletMeta_Subwallet::shouldLoad() const
{
   return false;
}

////////////////////////////////////////////////////////////////////////////////
shared_ptr<WalletMeta> WalletMeta::deserialize(
   shared_ptr<LMDBEnv> env, BinaryDataRef key, BinaryDataRef val)
{
   if (key.getSize() < 2)
      throw WalletException("invalid meta key");

   BinaryRefReader brrKey(key);
   auto prefix = brrKey.get_uint8_t();
   if (prefix != WALLETMETA_PREFIX)
      throw WalletException("invalid wallet meta prefix");

   string dbname((char*)brrKey.getCurrPtr(), brrKey.getSizeRemaining());

   BinaryRefReader brrVal(val);
   auto wltType = (WalletMetaType)brrVal.get_uint32_t();

   shared_ptr<WalletMeta> wltMetaPtr;

   switch (wltType)
   {
   case WalletMetaType_Single:
   {
      wltMetaPtr = make_shared<WalletMeta_Single>(env);
      wltMetaPtr->unseralizeVersion(brrVal);
      wltMetaPtr->unserializeEncryptionKey(brrVal);
      break;
   }

   case WalletMetaType_Subwallet:
   {
      wltMetaPtr = make_shared<WalletMeta_Subwallet>(env);
      break;
   }

   case WalletMetaType_Multisig:
   {
      wltMetaPtr = make_shared<WalletMeta_Multisig>(env);
      wltMetaPtr->unseralizeVersion(brrVal);
      wltMetaPtr->unserializeEncryptionKey(brrVal);
      break;
   }

   default:
      throw WalletException("invalid wallet type");
   }

   wltMetaPtr->dbName_ = move(dbname);
   wltMetaPtr->walletID_ = brrKey.get_BinaryData(brrKey.getSizeRemaining());
   return wltMetaPtr;
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
//// AssetWallet
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
AssetWallet::~AssetWallet()
{
   accounts_.clear();
   addresses_.clear();

   if (db_ != nullptr)
   {
      db_->close();
      delete db_;
      db_ = nullptr;
   }
}

////////////////////////////////////////////////////////////////////////////////
shared_ptr<AssetWallet_Single> AssetWallet_Single::
   createFromPrivateRoot_Armory135(
   const string& folder,
   const SecureBinaryData& privateRoot,
   const SecureBinaryData& passphrase,
   unsigned lookup)
{
   //compute wallet ID
   auto&& pubkey = CryptoECDSA().ComputePublicKey(privateRoot);
   
   //compute master ID as hmac256(root pubkey, "MetaEntry")
   string hmacMasterMsg("MetaEntry");
   auto&& masterID_long = BtcUtils::getHMAC256(
      pubkey, SecureBinaryData(hmacMasterMsg));
   auto&& masterID = BtcUtils::computeID(masterID_long);
   string masterIDStr(masterID.getCharPtr(), masterID.getSize());

   //create wallet file and dbenv
   stringstream pathSS;
   pathSS << folder << "/armory_" << masterIDStr << "_wallet.lmdb";
   auto dbenv = getEnvFromFile(pathSS.str(), 2);

   initWalletMetaDB(dbenv, masterIDStr);

   auto wltMetaPtr = make_shared<WalletMeta_Single>(dbenv);
   wltMetaPtr->parentID_ = masterID;

   {
      //walletID
      auto&& chaincode = BtcUtils::computeChainCode_Armory135(privateRoot);
      auto derScheme = make_shared<DerivationScheme_ArmoryLegacy>(
         chaincode);

      auto asset_single = make_shared<AssetEntry_Single>(
         ROOT_ASSETENTRY_ID, BinaryData(), 
         pubkey, nullptr);

      wltMetaPtr->walletID_ = move(computeWalletID(derScheme, asset_single));
   }
   
   //create kdf and master encryption key
   auto kdfPtr = make_shared<KeyDerivationFunction_Romix>();
   auto&& masterKeySBD = SecureBinaryData().GenerateRandom(32);
   DecryptedEncryptionKey masterEncryptionKey(masterKeySBD);
   masterEncryptionKey.deriveKey(kdfPtr);
   auto&& masterEncryptionKeyId = masterEncryptionKey.getId(kdfPtr->getId());

   auto cypher = make_unique<Cypher_AES>(kdfPtr->getId(), 
      masterEncryptionKeyId);

   SecureBinaryData dummy1, dummy2;
   auto&& privateRootCopy = privateRoot.copy();

   //address accounts
   set<shared_ptr<AccountType>> accountTypes;
   accountTypes.insert(
      make_shared<AccountType_ArmoryLegacy>(
      privateRootCopy, 
      dummy1, dummy2));
   (*accountTypes.begin())->setMain(true);
   
   auto walletPtr = initWalletDb(
      wltMetaPtr,
      kdfPtr,
      masterEncryptionKey,
      move(cypher),
      passphrase, 
      privateRoot, 
      move(accountTypes),
      lookup - 1);

   //set as main
   {
      LMDB dbMeta;

      {
         dbMeta.open(dbenv.get(), WALLETMETA_DBNAME);

        LMDBEnv::Transaction metatx(dbenv.get(), LMDB::ReadWrite);
        setMainWallet(&dbMeta, wltMetaPtr);
      }

      dbMeta.close();
   }

   return walletPtr;
}

////////////////////////////////////////////////////////////////////////////////
shared_ptr<AssetWallet_Single> AssetWallet_Single::
createFromPublicRoot_Armory135(
   const string& folder,
   SecureBinaryData& pubRoot,
   SecureBinaryData& chainCode,
   unsigned lookup)
{
   //compute master ID as hmac256(root pubkey, "MetaEntry")
   string hmacMasterMsg("MetaEntry");
   auto&& masterID_long = BtcUtils::getHMAC256(
      pubRoot, SecureBinaryData(hmacMasterMsg));
   auto&& masterID = BtcUtils::computeID(masterID_long);
   string masterIDStr(masterID.getCharPtr(), masterID.getSize());

   //create wallet file and dbenv
   stringstream pathSS;
   pathSS << folder << "/armory_" << masterIDStr << "_WatchingOnly.lmdb";
   auto dbenv = getEnvFromFile(pathSS.str(), 2);

   initWalletMetaDB(dbenv, masterIDStr);

   auto wltMetaPtr = make_shared<WalletMeta_Single>(dbenv);
   wltMetaPtr->parentID_ = masterID;

   {
      //walletID
      auto chainCode_copy = chainCode;
      auto derScheme = make_shared<DerivationScheme_ArmoryLegacy>(
         chainCode_copy);

      auto asset_single = make_shared<AssetEntry_Single>(
         ROOT_ASSETENTRY_ID, BinaryData(),
         pubRoot, nullptr);

      wltMetaPtr->walletID_ = move(computeWalletID(derScheme, asset_single));
   }

   //address accounts
   SecureBinaryData dummy;
   auto&& pubRootCopy = pubRoot.copy();
   auto&& chainCodeCopy = chainCode.copy();

   set<shared_ptr<AccountType>> accountTypes;
   accountTypes.insert(
      make_shared<AccountType_ArmoryLegacy>(
      dummy, pubRootCopy, chainCodeCopy));
   (*accountTypes.begin())->setMain(true);

   auto walletPtr = initWalletDbFromPubRoot(
      wltMetaPtr,
      pubRoot, 
      accountTypes,
      lookup - 1);

   //set as main
   {
      LMDB dbMeta;

      {
         dbMeta.open(dbenv.get(), WALLETMETA_DBNAME);

         LMDBEnv::Transaction metatx(dbenv.get(), LMDB::ReadWrite);
         setMainWallet(&dbMeta, wltMetaPtr);
      }

      dbMeta.close();
   }

   return walletPtr;
}

////////////////////////////////////////////////////////////////////////////////
shared_ptr<AssetWallet_Single> AssetWallet_Single::createFromSeed_BIP32(
   const string& folder,
   const SecureBinaryData& seed,
   const vector<unsigned>& derivationPath,
   const SecureBinaryData& passphrase,
   unsigned lookup)
{
   BIP32_Node rootNode;
   rootNode.initFromSeed(seed);

   //compute wallet ID
   auto pubkey = rootNode.getPublicKey();

   //compute master ID as hmac256(root pubkey, "MetaEntry")
   string hmacMasterMsg("MetaEntry");
   auto&& masterID_long = BtcUtils::getHMAC256(
      pubkey, SecureBinaryData(hmacMasterMsg));
   auto&& masterID = BtcUtils::computeID(masterID_long);
   string masterIDStr(masterID.getCharPtr(), masterID.getSize());

   //create wallet file and dbenv
   stringstream pathSS;
   pathSS << folder << "/armory_" << masterIDStr << "_wallet.lmdb";
   auto dbenv = getEnvFromFile(pathSS.str(), 2);

   initWalletMetaDB(dbenv, masterIDStr);

   auto wltMetaPtr = make_shared<WalletMeta_Single>(dbenv);
   wltMetaPtr->parentID_ = masterID;

   {
      //walletID
      auto chaincode_copy = rootNode.getChaincode();
      auto derScheme = make_shared<DerivationScheme_ArmoryLegacy>(
         chaincode_copy);

      auto asset_single = make_shared<AssetEntry_Single>(
         ROOT_ASSETENTRY_ID, BinaryData(),
         pubkey, nullptr);

      wltMetaPtr->walletID_ = move(computeWalletID(derScheme, asset_single));
   }

   //create kdf and master encryption key
   auto kdfPtr = make_shared<KeyDerivationFunction_Romix>();
   auto&& masterKeySBD = SecureBinaryData().GenerateRandom(32);
   DecryptedEncryptionKey masterEncryptionKey(masterKeySBD);
   masterEncryptionKey.deriveKey(kdfPtr);
   auto&& masterEncryptionKeyId = masterEncryptionKey.getId(kdfPtr->getId());

   auto cypher = make_unique<Cypher_AES>(kdfPtr->getId(),
      masterEncryptionKeyId);

   //address accounts
   set<shared_ptr<AccountType>> accountTypes;

   /*
   Derive 2 hardcoded paths on top of the main derivatrionPath for
   this wallet, to support the default address chains for Armory operations
   */

   SecureBinaryData dummy1, dummy2, dummy3, dummy4;
   auto privateRootCopy_1 = rootNode.getPrivateKey();
   auto privateRootCopy_2 = rootNode.getPrivateKey();

   accountTypes.insert(
      make_shared<AccountType_BIP32_Legacy>(
         privateRootCopy_1,
         dummy1, dummy2,
         derivationPath, 0, 0));
   (*accountTypes.begin())->setMain(true);

   accountTypes.insert(
      make_shared<AccountType_BIP32_SegWit>(
         privateRootCopy_2,
         dummy3, dummy4,
         derivationPath, 0, 0));

   auto walletPtr = initWalletDb(
      wltMetaPtr,
      kdfPtr,
      masterEncryptionKey,
      move(cypher),
      passphrase,
      rootNode.getPrivateKey(),
      move(accountTypes),
      lookup - 1);

   //set as main
   {
      LMDB dbMeta;

      {
         dbMeta.open(dbenv.get(), WALLETMETA_DBNAME);

         LMDBEnv::Transaction metatx(dbenv.get(), LMDB::ReadWrite);
         setMainWallet(&dbMeta, wltMetaPtr);
      }

      dbMeta.close();
   }

   return walletPtr;
}

////////////////////////////////////////////////////////////////////////////////
shared_ptr<AssetWallet_Single> AssetWallet_Single::createFromBase58_BIP32(
   const string& folder,
   const SecureBinaryData& base58,
   const vector<unsigned>& derivationPath,
   const SecureBinaryData& passphrase,
   unsigned lookup)
{
   //setup node
   BIP32_Node node;
   node.initFromBase58(base58);

   bool isPublic = false;
   if (node.isPublic())
      isPublic = true;

   //compute wallet ID
   auto pubkey = node.getPublicKey();

   //compute master ID as hmac256(root pubkey, "MetaEntry")
   string hmacMasterMsg("MetaEntry");
   auto&& masterID_long = BtcUtils::getHMAC256(
      pubkey, SecureBinaryData(hmacMasterMsg));
   auto&& masterID = BtcUtils::computeID(masterID_long);
   string masterIDStr(masterID.getCharPtr(), masterID.getSize());

   //create wallet file and dbenv
   stringstream pathSS;
   if (!isPublic)
      pathSS << folder << "/armory_" << masterIDStr << "_wallet.lmdb";
   else
      pathSS << folder << "/armory_" << masterIDStr << "_WatchingOnly.lmdb";

   auto dbenv = getEnvFromFile(pathSS.str(), 2);
   initWalletMetaDB(dbenv, masterIDStr);

   auto wltMetaPtr = make_shared<WalletMeta_Single>(dbenv);
   wltMetaPtr->parentID_ = masterID;

   {
      //walletID
      auto chaincode_copy = node.getChaincode();
      auto derScheme = 
         make_shared<DerivationScheme_ArmoryLegacy>(chaincode_copy);

      auto asset_single = make_shared<AssetEntry_Single>(
         ROOT_ASSETENTRY_ID, BinaryData(),
         pubkey, nullptr);

      wltMetaPtr->walletID_ = move(computeWalletID(derScheme, asset_single));
   }

   //create kdf and master encryption key
   auto kdfPtr = make_shared<KeyDerivationFunction_Romix>();
   auto&& masterKeySBD = SecureBinaryData().GenerateRandom(32);
   DecryptedEncryptionKey masterEncryptionKey(masterKeySBD);
   masterEncryptionKey.deriveKey(kdfPtr);
   auto&& masterEncryptionKeyId = masterEncryptionKey.getId(kdfPtr->getId());

   auto cypher = make_unique<Cypher_AES>(kdfPtr->getId(),
      masterEncryptionKeyId);

   //address accounts
   set<shared_ptr<AccountType>> accountTypes;

   /*
   Unlike wallets setup from seeds, we do not make any assumption with those setup from
   a xpriv/xpub and only use what's provided in derivationPath. It is the caller's
   responsibility to run sanity checks.
   */

   shared_ptr<AssetWallet_Single> walletPtr = nullptr;

   if (!isPublic)
   {
      auto privateRootCopy = node.getPrivateKey();
      SecureBinaryData dummy;
      auto chainCodeCopy = node.getChaincode();

      accountTypes.insert(make_shared<AccountType_BIP32_Custom>(
         privateRootCopy, dummy, chainCodeCopy, derivationPath,
         node.getDepth(), node.getLeafID()));
      (*accountTypes.begin())->setMain(true);

      walletPtr = initWalletDb(
         wltMetaPtr,
         kdfPtr,
         masterEncryptionKey,
         move(cypher),
         passphrase,
         node.getPrivateKey(),
         move(accountTypes),
         lookup - 1);
   }
   else
   {
      //ctors move the arguments in, gotta create copies first
      auto pubkey_copy = node.getPublicKey();
      auto chaincode_copy = node.getChaincode();
      SecureBinaryData dummy1;

      accountTypes.insert(make_shared<AccountType_BIP32_Custom>(
         dummy1, pubkey_copy, chaincode_copy, derivationPath,
         node.getDepth(), node.getLeafID()));
      (*accountTypes.begin())->setMain(true);

      pubkey_copy = node.getPublicKey();
      walletPtr = initWalletDbFromPubRoot(
         wltMetaPtr,
         pubkey_copy,
         move(accountTypes),
         lookup - 1);
   }

   //set as main
   {
      LMDB dbMeta;

      {
         dbMeta.open(dbenv.get(), WALLETMETA_DBNAME);

         LMDBEnv::Transaction metatx(dbenv.get(), LMDB::ReadWrite);
         setMainWallet(&dbMeta, wltMetaPtr);
      }

      dbMeta.close();
   }

   return walletPtr;
}

////////////////////////////////////////////////////////////////////////////////
shared_ptr<AssetWallet> AssetWallet::loadMainWalletFromFile(const string& path)
{
   auto dbenv = getEnvFromFile(path.c_str(), 1);

   unsigned count;
   map<BinaryData, shared_ptr<WalletMeta>> metaMap;
   BinaryData masterID;
   BinaryData mainWalletID;

   {
      {
         //db count and names
         count = getDbCountAndNames(
            dbenv, metaMap, masterID, mainWalletID);
      }
   }

   //close env, reopen env with proper count
   dbenv.reset();

   auto metaIter = metaMap.find(mainWalletID);
   if (metaIter == metaMap.end())
      throw WalletException("invalid main wallet id");

   auto mainWltMeta = metaIter->second;
   metaMap.clear();

   mainWltMeta->dbEnv_ = getEnvFromFile(path.c_str(), count + 1);
   
   shared_ptr<AssetWallet> wltPtr;

   switch (mainWltMeta->type_)
   {
   case WalletMetaType_Single:
   {
      auto wltSingle = make_shared<AssetWallet_Single>(mainWltMeta);
      wltSingle->readFromFile();

      wltPtr = wltSingle;
      break;
   }

   case WalletMetaType_Multisig:
   {
      auto wltMS = make_shared<AssetWallet_Multisig>(mainWltMeta);
      wltMS->readFromFile();

      wltPtr = wltMS;
      break;
   }

   default: 
      throw WalletException("unexpected main wallet type");
   }

   return wltPtr;
}

////////////////////////////////////////////////////////////////////////////////
void AssetWallet::putDbName(LMDB* db, shared_ptr<WalletMeta> wltMetaPtr)
{
   auto&& key = wltMetaPtr->getDbKey();
   auto&& val = wltMetaPtr->serialize();

   putData(db, key, val);
}

////////////////////////////////////////////////////////////////////////////////
void AssetWallet::setMainWallet(LMDB* db, shared_ptr<WalletMeta> wltMetaPtr)
{
   BinaryWriter bwKey;
   bwKey.put_uint32_t(MAINWALLET_KEY);

   BinaryWriter bwData;
   bwData.put_var_int(wltMetaPtr->walletID_.getSize());
   bwData.put_BinaryData(wltMetaPtr->walletID_);

   putData(db, bwKey.getData(), bwData.getData());
}

////////////////////////////////////////////////////////////////////////////////
void AssetWallet::initWalletMetaDB(
   shared_ptr<LMDBEnv> dbenv, const string& masterID)
{
   LMDB db;
   {
      db.open(dbenv.get(), WALLETMETA_DBNAME);

      BinaryWriter bwKey;
      bwKey.put_uint32_t(MASTERID_KEY);

      BinaryWriter bwData;
      bwData.put_var_int(masterID.size());

      BinaryDataRef idRef;
      idRef.setRef(masterID);
      bwData.put_BinaryDataRef(idRef);

      LMDBEnv::Transaction tx(dbenv.get(), LMDB::ReadWrite);
      putData(&db, bwKey.getData(), bwData.getData());
   }

   db.close();
}

////////////////////////////////////////////////////////////////////////////////
unsigned AssetWallet::getDbCountAndNames(shared_ptr<LMDBEnv> dbEnv,
   map<BinaryData, shared_ptr<WalletMeta>>& metaMap,
   BinaryData& masterID, BinaryData& mainWalletID)
{
   if (dbEnv == nullptr)
      throw WalletException("invalid dbenv");

   unsigned dbcount = 0;

   LMDB db;
   db.open(dbEnv.get(), WALLETMETA_DBNAME);

   {
      LMDBEnv::Transaction tx(dbEnv.get(), LMDB::ReadOnly);

      {
         //masterID
         BinaryWriter bwKey;
         bwKey.put_uint32_t(MASTERID_KEY);

         try
         {
            masterID = getDataRefForKey(bwKey.getData(), &db);
         }
         catch (NoEntryInWalletException&)
         {
            throw runtime_error("missing masterID entry");
         }
      }

      {
         //mainWalletID
         BinaryWriter bwKey;
         bwKey.put_uint32_t(MAINWALLET_KEY);

         try
         {
            mainWalletID = getDataRefForKey(bwKey.getData(), &db);
         }
         catch (NoEntryInWalletException&)
         {
            throw runtime_error("missing main wallet entry");
         }
      }

      //meta map
      auto dbIter = db.begin();

      BinaryWriter bwKey;
      bwKey.put_uint8_t(WALLETMETA_PREFIX);
      CharacterArrayRef keyRef(bwKey.getSize(), bwKey.getData().getPtr());

      dbIter.seek(keyRef, LMDB::Iterator::Seek_GE);

      while (dbIter.isValid())
      {
         auto iterkey = dbIter.key();
         auto itervalue = dbIter.value();

         BinaryDataRef keyBDR((uint8_t*)iterkey.mv_data, iterkey.mv_size);
         BinaryDataRef valueBDR((uint8_t*)itervalue.mv_data, itervalue.mv_size);

         //check value's advertized size is packet size and strip it
         BinaryRefReader brrVal(valueBDR);
         auto valsize = brrVal.get_var_int();
         if (valsize != brrVal.getSizeRemaining())
            throw WalletException("entry val size mismatch");

         try
         {
            auto metaPtr = WalletMeta::deserialize(
               dbEnv,
               keyBDR,
               brrVal.get_BinaryDataRef(brrVal.getSizeRemaining()));

            dbcount++;
            if (metaPtr->shouldLoad())
               metaMap.insert(make_pair(
               metaPtr->getWalletID(), metaPtr));
         }
         catch (exception& e)
         {
            LOGERR << e.what();
            break;
         }

         dbIter.advance();
      }
   }

   db.close();
   return dbcount + 1;
}

////////////////////////////////////////////////////////////////////////////////
BinaryData AssetWallet_Single::computeWalletID(
   shared_ptr<DerivationScheme> derScheme,
   shared_ptr<AssetEntry> rootEntry)
{
   auto&& addrVec = derScheme->extendPublicChain(rootEntry, 1, 1);
   if (addrVec.size() != 1)
      throw WalletException("unexpected chain derivation output");

   auto firstEntry = dynamic_pointer_cast<AssetEntry_Single>(addrVec[0]);
   if (firstEntry == nullptr)
      throw WalletException("unexpected asset entry type");

   return BtcUtils::computeID(firstEntry->getPubKey()->getUncompressedKey());
}

////////////////////////////////////////////////////////////////////////////////
shared_ptr<AssetWallet_Single> AssetWallet_Single::initWalletDb(
   shared_ptr<WalletMeta> metaPtr,
   shared_ptr<KeyDerivationFunction> masterKdf,
   DecryptedEncryptionKey& masterEncryptionKey,
   unique_ptr<Cypher> cypher,
   const SecureBinaryData& passphrase,
   const SecureBinaryData& privateRoot,
   set<shared_ptr<AccountType>> accountTypes,
   unsigned lookup)
{
   //create root AssetEntry
   auto&& pubkey = CryptoECDSA().ComputePublicKey(privateRoot);

   //copy cypher to cycle the IV then encrypt the private root
   masterEncryptionKey.deriveKey(masterKdf);
   auto&& masterEncryptionKeyId = masterEncryptionKey.getId(masterKdf->getId());
   auto&& rootCypher = cypher->getCopy(masterEncryptionKeyId);
   auto&& encryptedRoot = rootCypher->encrypt(
      &masterEncryptionKey, masterKdf->getId(), privateRoot);

   //create encrypted object
   auto rootAsset = make_shared<Asset_PrivateKey>(
      -1, encryptedRoot, move(rootCypher));

   auto rootAssetEntry = make_shared<AssetEntry_Single>(
      -1, BinaryData(),
      pubkey, rootAsset);

   if (metaPtr->dbName_.size() == 0)
   {
      string walletIDStr(metaPtr->getWalletIDStr());
      metaPtr->dbName_ = walletIDStr;
   }

   //encrypt master key, create object and set it
   metaPtr->defaultEncryptionKey_ = move(SecureBinaryData().GenerateRandom(32));
   auto defaultKey = metaPtr->getDefaultEncryptionKey();
   auto defaultEncryptionKeyPtr = make_unique<DecryptedEncryptionKey>(defaultKey);
   defaultEncryptionKeyPtr->deriveKey(masterKdf);
   metaPtr->defaultEncryptionKeyId_ =
      defaultEncryptionKeyPtr->getId(masterKdf->getId());

   //encrypt master encryption key with passphrase if present, otherwise use default
   unique_ptr<DecryptedEncryptionKey> topEncryptionKey;
   if (passphrase.getSize() > 0)
   {
      //copy passphrase
      auto&& passphraseCopy = passphrase.copy();
      topEncryptionKey = make_unique<DecryptedEncryptionKey>(passphraseCopy);
   }
   else
   {
      topEncryptionKey = move(defaultEncryptionKeyPtr);
   }

   topEncryptionKey->deriveKey(masterKdf);
   auto&& topEncryptionKeyId = topEncryptionKey->getId(masterKdf->getId());
   auto&& masterKeyCypher = cypher->getCopy(topEncryptionKeyId);
   auto&& encrMasterKey = masterKeyCypher->encrypt(
      topEncryptionKey.get(),
      masterKdf->getId(),
      masterEncryptionKey.getData());

   auto masterKeyPtr = make_shared<Asset_EncryptionKey>(masterEncryptionKeyId,
      encrMasterKey, move(masterKeyCypher));

   auto walletPtr = make_shared<AssetWallet_Single>(metaPtr);

   //add kdf & master key
   walletPtr->decryptedData_->addKdf(masterKdf);
   walletPtr->decryptedData_->addEncryptionKey(masterKeyPtr);

   //set passphrase lambda if necessary
   if (passphrase.getSize() > 0)
   {
      //custom passphrase, set prompt lambda for the chain extention
      auto passphraseLambda =
         [&passphrase](const BinaryData&)->SecureBinaryData
      {
         return passphrase;
      };

      walletPtr->decryptedData_->setPassphrasePromptLambda(passphraseLambda);
   }


   {
      LMDB metadb;

      {
         metadb.open(walletPtr->dbEnv_.get(), WALLETMETA_DBNAME);

         LMDBEnv::Transaction tx(walletPtr->dbEnv_.get(), LMDB::ReadWrite);
         putDbName(&metadb, metaPtr);
      }

      metadb.close();
   }


   /**insert the original entries**/
   {
      LMDBEnv::Transaction tx(walletPtr->dbEnv_.get(), LMDB::ReadWrite);
      walletPtr->putHeaderData(
         metaPtr->parentID_, metaPtr->walletID_);

      {
         //decrypted data container
         walletPtr->decryptedData_->updateOnDisk();
      }

      {
         //root asset
         BinaryWriter bwKey;
         bwKey.put_uint32_t(ROOTASSET_KEY);

         auto&& data = rootAssetEntry->serialize();

         walletPtr->putData(bwKey.getData(), data);
      }

      {
         //accounts
         for (auto& accountType : accountTypes)
         {
            //instantiate AddressAccount object from AccountType
            auto account_ptr = make_shared<AddressAccount>(
               walletPtr->dbEnv_,
               walletPtr->db_);

            auto&& cypher_copy = cypher->getCopy();
            account_ptr->make_new(accountType,
               walletPtr->decryptedData_,
               move(cypher_copy));

            //commit to disk
            account_ptr->commit();

            if (accountType->isMain())
               walletPtr->mainAccount_ = account_ptr->getID();
         }

         //main account
         if (walletPtr->mainAccount_.getSize() > 0)
         {
            BinaryWriter bwKey;
            bwKey.put_uint32_t(MAIN_ACCOUNT_KEY);

            BinaryWriter bwData;
            bwData.put_var_int(walletPtr->mainAccount_.getSize());
            bwData.put_BinaryData(walletPtr->mainAccount_);
            walletPtr->putData(bwKey.getData(), bwData.getData());
         }
      }
   }

   //init walletptr from file
   walletPtr->readFromFile();

   {
      //asset lookup
      if (lookup == UINT32_MAX)
         lookup = DERIVATION_LOOKUP;

      walletPtr->extendPrivateChain(lookup);
   }

   walletPtr->decryptedData_->resetPassphraseLambda();
   return walletPtr;
}

////////////////////////////////////////////////////////////////////////////////
shared_ptr<AssetWallet_Single> AssetWallet_Single::initWalletDbFromPubRoot(
   shared_ptr<WalletMeta> metaPtr,
   SecureBinaryData& pubRoot,
   set<shared_ptr<AccountType>> accountTypes,
   unsigned lookup)
{
   //create root AssetEntry
   auto rootAssetEntry = make_shared<AssetEntry_Single>(
      -1, BinaryData(),
      pubRoot, nullptr);

   if (metaPtr->dbName_.size() == 0)
   {
      string walletIDStr(metaPtr->getWalletIDStr());
      metaPtr->dbName_ = walletIDStr;
   }

   auto walletPtr = make_shared<AssetWallet_Single>(metaPtr);

   {
      LMDB metadb;

      {
         metadb.open(walletPtr->dbEnv_.get(), WALLETMETA_DBNAME);

         LMDBEnv::Transaction tx(walletPtr->dbEnv_.get(), LMDB::ReadWrite);
         putDbName(&metadb, metaPtr);
      }

      metadb.close();
   }

   /**insert the original entries**/
   {
      LMDBEnv::Transaction tx(walletPtr->dbEnv_.get(), LMDB::ReadWrite);
      walletPtr->putHeaderData(
         metaPtr->parentID_, metaPtr->walletID_);

      {
         //root asset
         BinaryWriter bwKey;
         bwKey.put_uint32_t(ROOTASSET_KEY);

         auto&& data = rootAssetEntry->serialize();

         walletPtr->putData(bwKey.getData(), data);
      }

      {
         //accounts
         for (auto& accountType : accountTypes)
         {
            //instantiate AddressAccount object from AccountType
            auto account_ptr = make_shared<AddressAccount>(
               walletPtr->dbEnv_,
               walletPtr->db_);

            account_ptr->make_new(accountType, nullptr, nullptr);

            //commit to disk
            account_ptr->commit();

            if (accountType->isMain())
               walletPtr->mainAccount_ = account_ptr->getID();
         }

         //main account
         if (walletPtr->mainAccount_.getSize() > 0)
         {
            BinaryWriter bwKey;
            bwKey.put_uint32_t(MAIN_ACCOUNT_KEY);

            BinaryWriter bwData;
            bwData.put_var_int(walletPtr->mainAccount_.getSize());
            bwData.put_BinaryData(walletPtr->mainAccount_);
            walletPtr->putData(bwKey.getData(), bwData.getData());
         }
      }
   }

   //init walletptr from file
   walletPtr->readFromFile();

   {
      //asset lookup
      auto topEntryPtr = rootAssetEntry;

      if (lookup == UINT32_MAX)
         lookup = DERIVATION_LOOKUP;

      walletPtr->extendPublicChain(lookup);
   }

   return walletPtr;
}

////////////////////////////////////////////////////////////////////////////////
void AssetWallet_Single::putHeaderData(
   const BinaryData& parentID,
   const BinaryData& walletID)
{
   LMDBEnv::Transaction tx(dbEnv_.get(), LMDB::ReadWrite);

   {
      //wallet type
      BinaryWriter bwKey;
      bwKey.put_uint32_t(WALLETTYPE_KEY);

      BinaryWriter bwData;
      bwData.put_var_int(4);
      bwData.put_uint32_t(WalletMetaType_Single);

      putData(bwKey, bwData);
   }

   AssetWallet::putHeaderData(parentID, walletID);
}

////////////////////////////////////////////////////////////////////////////////
void AssetWallet::putHeaderData(
   const BinaryData& parentID,
   const BinaryData& walletID)
{
   LMDBEnv::Transaction tx(dbEnv_.get(), LMDB::ReadWrite);

   {
      //parent ID
      BinaryWriter bwKey;
      bwKey.put_uint32_t(PARENTID_KEY);

      BinaryWriter bwData;
      bwData.put_var_int(parentID.getSize());
      bwData.put_BinaryData(parentID);

      putData(bwKey, bwData);
   }

   {
      //wallet ID
      BinaryWriter bwKey;
      bwKey.put_uint32_t(WALLETID_KEY);

      BinaryWriter bwData;
      bwData.put_var_int(walletID.getSize());
      bwData.put_BinaryData(walletID);

      putData(bwKey, bwData);
   }
}

////////////////////////////////////////////////////////////////////////////////
BinaryDataRef AssetWallet::getDataRefForKey(const BinaryData& key) const
{
   /** The reference lifetime is tied to the db tx lifetime. The caller has to
   maintain the tx for as long as the data ref needs to be valid **/

   return getDataRefForKey(key, db_);
}

////////////////////////////////////////////////////////////////////////////////
BinaryDataRef AssetWallet::getDataRefForKey(const BinaryData& key, LMDB* db)
{
   CharacterArrayRef keyRef(key.getSize(), key.getPtr());
   auto ref = db->get_NoCopy(keyRef);

   if (ref.data == nullptr)
      throw NoEntryInWalletException();

   return DBUtils::getDataRefForPacket(
      BinaryDataRef((uint8_t*)ref.data, ref.len));
}

////////////////////////////////////////////////////////////////////////////////
void AssetWallet_Single::readFromFile()
{
   //sanity check
   if (dbEnv_ == nullptr || db_ == nullptr)
      throw WalletException("uninitialized wallet object");

   LMDBEnv::Transaction tx(dbEnv_.get(), LMDB::ReadOnly);

   {
      //parentId
      BinaryWriter bwKey;
      bwKey.put_uint32_t(PARENTID_KEY);

      auto parentIdRef = getDataRefForKey(bwKey.getData());
      parentID_ = parentIdRef;
   }

   {
      //walletId
      BinaryWriter bwKey;
      bwKey.put_uint32_t(WALLETID_KEY);
      auto walletIdRef = getDataRefForKey(bwKey.getData());

      walletID_ = walletIdRef;
   }

   {
      //main account
      BinaryWriter bwKey;
      bwKey.put_uint32_t(MAIN_ACCOUNT_KEY);

      try
      {
         auto account_id = getDataRefForKey(bwKey.getData());

         mainAccount_ = account_id;
      }
      catch (NoEntryInWalletException&)
      { }
   }

   {
      //root asset
      BinaryWriter bwKey;
      bwKey.put_uint32_t(ROOTASSET_KEY);
      auto rootAssetRef = getDataRefForKey(bwKey.getData());

      auto asset_root = AssetEntry::deserDBValue(-1, BinaryData(), rootAssetRef);
      root_ = dynamic_pointer_cast<AssetEntry_Single>(asset_root);
   }

   //encryption keys and kdfs
   decryptedData_->readFromDisk();

   {
      //accounts
      BinaryWriter bwPrefix;
      bwPrefix.put_uint8_t(ADDRESS_ACCOUNT_PREFIX);
      CharacterArrayRef account_prefix(
         bwPrefix.getSize(), bwPrefix.getData().getCharPtr());

      auto dbIter = db_->begin();
      dbIter.seek(account_prefix, LMDB::Iterator::Seek_GE);

      while (dbIter.isValid())
      {
         //iterate through account keys
         auto& key = dbIter.key();
         BinaryData key_bd((uint8_t*)key.mv_data, key.mv_size);

         try
         {
            //instantiate account object and read data on disk
            auto addressAccount = make_shared<AddressAccount>(dbEnv_, db_);
            addressAccount->readFromDisk(key_bd);

            //insert
            accounts_.insert(addressAccount);
         }
         catch (exception&)
         {
            //in case of exception, the value for this key is not for an
            //account. Assume we ran out of accounts and break out.
            break;
         }

         ++dbIter;
      }
   }
}

////////////////////////////////////////////////////////////////////////////////
void AssetWallet_Multisig::readFromFile()
{
   //sanity check
   if (dbEnv_ == nullptr || db_ == nullptr)
      throw WalletException("uninitialized wallet object");

   {
      LMDBEnv::Transaction tx(dbEnv_.get(), LMDB::ReadOnly);

      {
         //parentId
         BinaryWriter bwKey;
         bwKey.put_uint32_t(PARENTID_KEY);

         auto parentIdRef = getDataRefForKey(bwKey.getData());
         parentID_ = parentIdRef;
      }

      {
         //walletId
         BinaryWriter bwKey;
         bwKey.put_uint32_t(WALLETID_KEY);
         auto walletIdRef = getDataRefForKey(bwKey.getData());

         walletID_ = walletIdRef;
      }

      {
         //lookup
         {
            BinaryWriter bwKey;
            bwKey.put_uint8_t(ASSETENTRY_PREFIX);
            auto lookupRef = getDataRefForKey(bwKey.getData());

            BinaryRefReader brr(lookupRef);
            chainLength_ = brr.get_uint32_t();
         }
      }
   }

   {
      unsigned n = 0;

      map<BinaryData, shared_ptr<AssetWallet_Single>> walletPtrs;
      for (unsigned i = 0; i < n; i++)
      {
         stringstream ss;
         ss << "Subwallet-" << i;

         auto subWltMeta = make_shared<WalletMeta_Subwallet>(dbEnv_);
         subWltMeta->dbName_ = ss.str();

         auto subwalletPtr = make_shared<AssetWallet_Single>(subWltMeta);
         subwalletPtr->readFromFile();
         walletPtrs[subwalletPtr->getID()] = subwalletPtr;

      }
   }
}

////////////////////////////////////////////////////////////////////////////////
void AssetWallet::putData(const BinaryData& key, const BinaryData& data)
{
   /** the caller is responsible for the db transaction **/
   putData(db_, key, data);
}

////////////////////////////////////////////////////////////////////////////////
void AssetWallet::putData(
   LMDB* db, const BinaryData& key, const BinaryData& data)
{
   CharacterArrayRef keyRef(key.getSize(), key.getPtr());
   CharacterArrayRef dataRef(data.getSize(), data.getPtr());

   db->insert(keyRef, dataRef);
}

////////////////////////////////////////////////////////////////////////////////
void AssetWallet::putData(BinaryWriter& key, BinaryWriter& data)
{
   putData(key.getData(), data.getData());
}

////////////////////////////////////////////////////////////////////////////////
shared_ptr<AddressEntry> AssetWallet::getNewAddress(
   AddressEntryType aeType)
{
   //lock
   ReentrantLock lock(this);

   if (mainAccount_.getSize() == 0)
      throw WalletException("no main account for wallet");

   return getNewAddress(mainAccount_, aeType);
}

////////////////////////////////////////////////////////////////////////////////
shared_ptr<AddressEntry> AssetWallet::getNewAddress(
   const BinaryData& accountID, AddressEntryType aeType)
{
   ReentrantLock lock(this);

   auto account = getAccountForID(accountID);
   auto newAddress = account->getNewAddress(aeType);
   updateAddressSet(newAddress);

   return newAddress;
}

////////////////////////////////////////////////////////////////////////////////
void AssetWallet::updateAddressSet(shared_ptr<AddressEntry> addrPtr)
{
   /***
   While AddressEntry objects are requested from Accounts, Accounts themselves
   do not keep track of address instantiation. Instead, wallets keep track of 
   that with a simple kay:val scheme:

   (ADDRESS_PREFIX|Asset's ID):(AddressEntry type)
   ***/

   //only commit to disk if the addr is missing or the type differs
   auto iter = addresses_.find(addrPtr->getID());
   if (iter != addresses_.end())
   {
      if (iter->second->getType() == addrPtr->getType())
         return;
   }

   addresses_[addrPtr->getID()] = addrPtr;
   writeAddressType(addrPtr);
}

////////////////////////////////////////////////////////////////////////////////
void AssetWallet::writeAddressType(shared_ptr<AddressEntry> addrPtr)
{
   ReentrantLock lock(this);

   BinaryWriter bwKey;
   bwKey.put_uint8_t(ADDRESS_TYPE_PREFIX);
   bwKey.put_BinaryData(addrPtr->getID());

   BinaryWriter bwData;
   bwData.put_uint8_t(addrPtr->getType());

   CharacterArrayRef carKey(bwKey.getSize(), bwKey.getData().getCharPtr());
   CharacterArrayRef carData(bwData.getSize(), bwData.getData().getCharPtr());

   LMDBEnv::Transaction tx(dbEnv_.get(), LMDB::ReadWrite);
   db_->insert(carKey, carData);
}

////////////////////////////////////////////////////////////////////////////////
bool AssetWallet::hasScrAddr(const BinaryData& scrAddr)
{
   try
   {
      getAssetIDForAddr(scrAddr);
   }
   catch (runtime_error&)
   {
      return false;
   }

   return true;
}

////////////////////////////////////////////////////////////////////////////////
const pair<BinaryData, AddressEntryType>& 
   AssetWallet::getAssetIDForAddr(const BinaryData& scrAddr)
{
   //this takes prefixed hashes or a b58 address

   ReentrantLock lock(this);
   
   BinaryData scrHash;

   try
   {
      scrHash = move(BtcUtils::base58toScrAddr(scrAddr));
   }
   catch(runtime_error&)
   {
      scrHash = scrAddr;
   }

   for (auto acc : accounts_)
   {
      try
      {
         return acc->getAssetIDPairForAddr(scrHash);
      }
      catch (runtime_error&)
      {
         continue;
      }
   }

   throw runtime_error("unknown scrAddr");
}

////////////////////////////////////////////////////////////////////////////////
AddressEntryType AssetWallet::getAddrTypeForID(const BinaryData& ID)
{
   ReentrantLock lock(this);
   
   auto addrIter = addresses_.find(ID);
   if (addrIter != addresses_.end())
      return addrIter->second->getType();

   return getAddrTypeForAccount(ID);
}

////////////////////////////////////////////////////////////////////////////////
shared_ptr<AddressAccount> AssetWallet::getAccountForID(
   const BinaryData& ID) const
{

   auto iter = find_if(
      accounts_.begin(), accounts_.end(), 
      AddressAccount::find_by_id(ID));

   if (iter == accounts_.end())
      throw WalletException("unknown account ID");

   return *iter;
}

////////////////////////////////////////////////////////////////////////////////
AddressEntryType AssetWallet::getAddrTypeForAccount(const BinaryData& ID)
{
   auto acc = getAccountForID(ID);
   return acc->getAddressType();
}

////////////////////////////////////////////////////////////////////////////////
shared_ptr<AddressEntry> AssetWallet::getAddressEntryForAsset(
   shared_ptr<AssetEntry> assetPtr, AddressEntryType ae_type)
{
   ReentrantLock lock(this);
   
   auto addrIter = addresses_.find(assetPtr->getID());
   if (addrIter != addresses_.end())
   {
      if(addrIter->second->getType() == ae_type)
         return addrIter->second;
   }

   auto acc = getAccountForID(assetPtr->getID());
   if (!acc->hasAddressType(ae_type))
      throw WalletException("invalid address type for account");

   auto addrPtr = AddressEntry::instantiate(assetPtr, ae_type);
   updateAddressSet(addrPtr);
   return addrPtr;
}

////////////////////////////////////////////////////////////////////////////////
shared_ptr<AddressEntry> AssetWallet::getAddressEntryForID(
   const BinaryData& ID, AddressEntryType aeType)
{
   ReentrantLock lock(this);

   auto addrIter = addresses_.find(ID);
   if (addrIter != addresses_.end())
   {
      if (aeType != AddressEntryType_Default && 
          aeType == addrIter->second->getType())
      return addrIter->second;
   }

   if (aeType == AddressEntryType_Default)
   {
      auto acc = getAccountForID(ID);
      aeType = acc->getAddressType();
   }

   auto asset = getAssetForID(ID);
   return getAddressEntryForAsset(asset, aeType);
}

////////////////////////////////////////////////////////////////////////////////
const string& AssetWallet::getFilename() const
{
   if (dbEnv_ == nullptr)
      throw runtime_error("null dbenv");

   return dbEnv_->getFilename();
}

////////////////////////////////////////////////////////////////////////////////
void AssetWallet::updateHashMap()
{
   ReentrantLock lock(this);

   for (auto account : accounts_)
      account->updateAddressHashMap();
}

////////////////////////////////////////////////////////////////////////////////
set<BinaryData> AssetWallet::getAddrHashSet()
{
   ReentrantLock lock(this);

   set<BinaryData> addrHashSet;
   for (auto account : accounts_)
   {
      auto& hashes = account->getAddressHashMap();

      for (auto& hashPair : hashes)
         addrHashSet.insert(hashPair.first);
   }

   return addrHashSet;
}

////////////////////////////////////////////////////////////////////////////////
shared_ptr<AssetEntry> AssetWallet::getAssetForID(const BinaryData& ID) const
{
   if (ID.getSize() < 8)
      throw WalletException("invalid asset ID");

   ReentrantLock lock(this);

   auto acc = getAccountForID(ID);
   return acc->getAssetForID(ID.getSliceRef(4, ID.getSize() - 4));
}

////////////////////////////////////////////////////////////////////////////////
string AssetWallet::getID(void) const
{
   return string(walletID_.getCharPtr(), walletID_.getSize());
}

////////////////////////////////////////////////////////////////////////////////
ReentrantLock AssetWallet::lockDecryptedContainer(void)
{
   return move(ReentrantLock(decryptedData_.get()));
}

////////////////////////////////////////////////////////////////////////////////
bool AssetWallet::isDecryptedContainerLocked() const
{
   try
   {
      auto lock = SingleLock(decryptedData_.get());
      return false;
   }
   catch (AlreadyLocked&)
   {}

   return true;
}

////////////////////////////////////////////////////////////////////////////////
void AssetWallet::extendPublicChain(unsigned count)
{
   for (auto& account : accounts_)
   {
      account->extendPublicChain(count);
   }
}

////////////////////////////////////////////////////////////////////////////////
void AssetWallet::extendPrivateChain(unsigned count)
{
   for (auto& account : accounts_)
   {
      account->extendPrivateChain(decryptedData_, count);
   }
}

////////////////////////////////////////////////////////////////////////////////
void AssetWallet::extendPublicChainToIndex(
   const BinaryData& account_id, unsigned count)
{
   auto account = getAccountForID(account_id);
   account->extendPublicChainToIndex(
      account->getOuterAccount()->getID(), count);
}

////////////////////////////////////////////////////////////////////////////////
void AssetWallet::extendPrivateChainToIndex(
   const BinaryData& account_id, unsigned count)
{
   auto account = getAccountForID(account_id);
   account->extendPrivateChainToIndex(
      decryptedData_,
      account->getOuterAccount()->getID(), count);
}

////////////////////////////////////////////////////////////////////////////////
const SecureBinaryData& AssetWallet_Single::getDecryptedValue(
   shared_ptr<Asset_PrivateKey> assetPtr)
{
   //have to lock the decryptedData object before calling this method or it 
   //will throw
   return decryptedData_->getDecryptedPrivateKey(assetPtr);
}

////////////////////////////////////////////////////////////////////////////////
const SecureBinaryData& AssetWallet_Multisig::getDecryptedValue(
   shared_ptr<Asset_PrivateKey> assetPtr)
{
   return decryptedData_->getDecryptedPrivateKey(assetPtr);
}

////////////////////////////////////////////////////////////////////////////////
void AssetWallet_Single::changeMasterPassphrase(const SecureBinaryData& newPassphrase)
{
   auto lock = lockDecryptedContainer();
   auto&& masterKeyId = root_->getPrivateEncryptionKeyId();
   decryptedData_->encryptEncryptionKey(masterKeyId, newPassphrase);
}

////////////////////////////////////////////////////////////////////////////////
const SecureBinaryData& AssetWallet_Single::getPublicRoot() const
{
   if (root_ == nullptr)
      throw WalletException("null root");

   auto pubkey = root_->getPubKey();
   if (pubkey == nullptr)
      throw WalletException("null pubkey");

   return pubkey->getUncompressedKey();
}

////////////////////////////////////////////////////////////////////////////////
const SecureBinaryData& AssetWallet_Single::getArmory135Chaincode() const
{
   BinaryWriter bw;
   bw.put_uint32_t(ARMORY_LEGACY_ACCOUNTID, BE);

   auto account = getAccountForID(bw.getData());
   auto assetAccount = account->getOuterAccount();
   return assetAccount->getChaincode();
}

////////////////////////////////////////////////////////////////////////////////
shared_ptr<AssetEntry> 
   AssetWallet_Single::getMainAccountAssetForIndex(unsigned id) const
{
   auto account = getAccountForID(mainAccount_);
   if (account == nullptr)
      throw WalletException("failed to grab main account");

   return account->getOutterAssetForIndex(id);
}

////////////////////////////////////////////////////////////////////////////////
unsigned AssetWallet_Single::getMainAccountAssetCount(void) const
{
   auto account = getAccountForID(mainAccount_);
   if (account == nullptr)
      throw WalletException("failed to grab main account");

   auto asset_account = account->getOuterAccount();
   return asset_account->getAssetCount();
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
shared_ptr<AssetEntry> AssetWallet_Single::getAccountRoot(
   const BinaryData& id) const
{
   auto account = getAccountForID(id);
   if (account == nullptr)
      throw WalletException("failed to grab main account");

   return account->getOutterAssetRoot();
}
