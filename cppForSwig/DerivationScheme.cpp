////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//  Copyright (C) 2017, goatpig                                               //
//  Distributed under the MIT license                                         //
//  See LICENSE-MIT or https://opensource.org/licenses/MIT                    //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

#include "ReentrantLock.h"
#include "DerivationScheme.h"
#include "DecryptedDataContainer.h"
#include "BIP32_Node.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
//// DerivationScheme
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
DerivationScheme::~DerivationScheme()
{}

////////////////////////////////////////////////////////////////////////////////
shared_ptr<DerivationScheme> DerivationScheme::deserialize(
   BinaryDataRef data, LMDB* db)
{
   BinaryRefReader brr(data);

   //get derivation scheme type
   auto schemeType = brr.get_uint8_t();

   shared_ptr<DerivationScheme> derScheme;

   switch (schemeType)
   {
   case DERIVATIONSCHEME_LEGACY:
   {
      //get chaincode;
      auto len = brr.get_var_int();
      auto&& chainCode = SecureBinaryData(brr.get_BinaryDataRef(len));
      derScheme = make_shared<DerivationScheme_ArmoryLegacy>(
         chainCode);

      break;
   }

   case DERIVATIONSCHEME_BIP32:
   {
      //chaincode;
      auto len = brr.get_var_int();
      auto&& chainCode = SecureBinaryData(brr.get_BinaryDataRef(len));

      //bip32 node meta data
      auto depth = brr.get_uint32_t();
      auto leafID = brr.get_uint32_t();

      //instantiate object
      derScheme = make_shared<DerivationScheme_BIP32>(
         chainCode, depth, leafID);

      break;
   }

   case DERIVATIONSCHEME_BIP32_SALTED:
   {
      //chaincode;
      auto len = brr.get_var_int();
      auto&& chainCode = SecureBinaryData(brr.get_BinaryDataRef(len));

      //bip32 node meta data
      auto depth = brr.get_uint32_t();
      auto leafID = brr.get_uint32_t();

      //salt
      len = brr.get_var_int();
      auto&& salt = SecureBinaryData(brr.get_BinaryDataRef(len));

      //instantiate object
      derScheme = make_shared<DerivationScheme_BIP32_Salted>(
         salt, chainCode, depth, leafID);

      break;

   }

   case DERIVATIONSCHEME_BIP32_ECDH:
   {
      //id
      auto len = brr.get_var_int();
      auto id = brr.get_BinaryData(len);

      //saltMap
      map<SecureBinaryData, unsigned> saltMap;

      BinaryWriter bwKey;
      bwKey.put_uint8_t(ECDH_SALT_PREFIX);
      bwKey.put_BinaryData(id);

      BinaryDataRef keyBdr = bwKey.getDataRef();
      CharacterArrayRef carKey(keyBdr.getSize(), keyBdr.getPtr());

      auto dbIter = db->begin();
      dbIter.seek(carKey, LMDB::Iterator::Seek_GE);
      while (dbIter.isValid())
      {
         auto& key = dbIter.key();
         BinaryDataRef key_bdr((uint8_t*)key.mv_data, key.mv_size);
         if (!key_bdr.startsWith(keyBdr) || 
             key_bdr.getSize() != keyBdr.getSize() + 4)
            break;

         auto saltIdBdr = key_bdr.getSliceCopy(keyBdr.getSize(), 4);
         auto saltId = READ_UINT32_BE(saltIdBdr);

         auto value = dbIter.value();
         BinaryDataRef value_bdr((uint8_t*)value.mv_data, value.mv_size);
         BinaryRefReader bdrData(value_bdr);
         auto len = bdrData.get_var_int();
         auto&& salt = bdrData.get_SecureBinaryData(len);

         saltMap.emplace(make_pair(move(salt), saltId));
         ++dbIter;
      }

      derScheme = make_shared<DerivationScheme_ECDH>(id, saltMap);
      break;
   }

   default:
      throw DerivationSchemeException("unsupported derivation scheme");
   }

   return derScheme;
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
//// DerivationScheme_ArmoryLegacy
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
shared_ptr<AssetEntry_Single>
   DerivationScheme_ArmoryLegacy::computeNextPublicEntry(
   const SecureBinaryData& pubKey,
   const BinaryData& accountID, unsigned index)
{
   auto&& nextPubkey = CryptoECDSA().ComputeChainedPublicKey(
      pubKey, chainCode_, nullptr);

   return make_shared<AssetEntry_Single>(
      index, accountID,
      nextPubkey, nullptr);
}

////////////////////////////////////////////////////////////////////////////////
vector<shared_ptr<AssetEntry>> DerivationScheme_ArmoryLegacy::extendPublicChain(
   shared_ptr<AssetEntry> firstAsset, 
   unsigned start, unsigned end)
{
   auto nextAsset = [this](
      shared_ptr<AssetEntry> assetPtr)->shared_ptr<AssetEntry>
   {
      auto assetSingle =
         dynamic_pointer_cast<AssetEntry_Single>(assetPtr);

      //get pubkey
      auto pubkey = assetSingle->getPubKey();
      auto& pubkeyData = pubkey->getUncompressedKey();

      return computeNextPublicEntry(pubkeyData,
         assetSingle->getAccountID(), assetSingle->getIndex() + 1);
   };

   vector<shared_ptr<AssetEntry>> assetVec;
   auto currentAsset = firstAsset;

   for (unsigned i = start; i <= end; i++)
   {
      currentAsset = nextAsset(currentAsset);
      assetVec.push_back(currentAsset);
   }

   return assetVec;
}

////////////////////////////////////////////////////////////////////////////////
shared_ptr<AssetEntry_Single> 
   DerivationScheme_ArmoryLegacy::computeNextPrivateEntry(
   shared_ptr<DecryptedDataContainer> ddc,
   const SecureBinaryData& privKeyData, unique_ptr<Cipher> cipher,
   const BinaryData& accountID, unsigned index)
{
   //chain the private key
   auto&& nextPrivkeySBD = CryptoECDSA().ComputeChainedPrivateKey(
      privKeyData, chainCode_);

   //compute its pubkey
   auto&& nextPubkey = CryptoECDSA().ComputePublicKey(nextPrivkeySBD);

   //encrypt the new privkey
   auto&& newCipher = cipher->getCopy(); //copying a cipher cycles the IV
   auto&& encryptedNextPrivKey = ddc->encryptData(
      newCipher.get(), nextPrivkeySBD);

   //clear the unencrypted privkey object
   nextPrivkeySBD.clear();

   //instantiate new encrypted key object
   auto privKeyID = accountID;
   privKeyID.append(WRITE_UINT32_BE(index));
   auto nextPrivKey = make_shared<Asset_PrivateKey>(
      privKeyID, encryptedNextPrivKey, move(newCipher));

   //instantiate and return new asset entry
   return make_shared<AssetEntry_Single>(
      index, accountID, nextPubkey, nextPrivKey);
}

////////////////////////////////////////////////////////////////////////////////
vector<shared_ptr<AssetEntry>> 
   DerivationScheme_ArmoryLegacy::extendPrivateChain(
   shared_ptr<DecryptedDataContainer> ddc,
   shared_ptr<AssetEntry> firstAsset, 
   unsigned start, unsigned end)
{
   //throws if the wallet is locked or the asset is missing its private key

   auto nextAsset = [this, ddc](
      shared_ptr<AssetEntry> assetPtr)->shared_ptr<AssetEntry>
   {
      //sanity checks
      auto assetSingle =
         dynamic_pointer_cast<AssetEntry_Single>(assetPtr);

      auto privkey = assetSingle->getPrivKey();
      if (privkey == nullptr)
         throw AssetUnavailableException();
      auto& privkeyData =
         ddc->getDecryptedPrivateData(privkey);

      auto id_int = assetSingle->getIndex() + 1;
      auto& account_id = assetSingle->getAccountID();

      return computeNextPrivateEntry(
         ddc, 
         privkeyData, move(privkey->getCipherDataPtr()->cipher_->getCopy()),
         account_id, id_int);
   };

   if (ddc == nullptr || firstAsset == nullptr)
   {
      LOGERR << "missing asset, cannot extent private chain";
      throw AssetUnavailableException();
   }

   ReentrantLock lock(ddc.get());

   vector<shared_ptr<AssetEntry>> assetVec;
   auto currentAsset = firstAsset;

   for (unsigned i = start; i <= end; i++)
   {
      currentAsset = nextAsset(currentAsset);
      assetVec.push_back(currentAsset);
   }

   return assetVec;
}

////////////////////////////////////////////////////////////////////////////////
BinaryData DerivationScheme_ArmoryLegacy::serialize() const
{
   BinaryWriter bw;
   bw.put_uint8_t(DERIVATIONSCHEME_LEGACY);
   bw.put_var_int(chainCode_.getSize());
   bw.put_BinaryData(chainCode_);

   BinaryWriter final;
   final.put_var_int(bw.getSize());
   final.put_BinaryData(bw.getData());

   return final.getData();
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
//// DerivationScheme_BIP32
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
shared_ptr<AssetEntry_Single>
   DerivationScheme_BIP32::computeNextPrivateEntry(
   shared_ptr<DecryptedDataContainer> ddc,
   const SecureBinaryData& privKeyData, unique_ptr<Cipher> cipher,
   const BinaryData& accountID, unsigned index)
{
   //derScheme only allows for soft derivation
   if (index > 0x7FFFFFFF)
      throw DerivationSchemeException("illegal: hard derivation");

   BIP32_Node node;
   node.initFromPrivateKey(depth_, leafId_, 0, privKeyData, chainCode_);
   node.derivePrivate(index);

   //encrypt the new privkey
   auto&& newCipher = cipher->getCopy(); //copying a cypher cycles the IV
   auto&& encryptedNextPrivKey = ddc->encryptData(
      newCipher.get(), node.getPrivateKey());

   //instantiate new encrypted key object
   auto privKeyID = accountID;
   privKeyID.append(WRITE_UINT32_BE(index));
   auto nextPrivKey = make_shared<Asset_PrivateKey>(
      privKeyID, encryptedNextPrivKey, move(newCipher));

   //instantiate and return new asset entry
   auto nextPubkey = node.movePublicKey();
   return make_shared<AssetEntry_Single>(
      index, accountID, nextPubkey, nextPrivKey);
}

////////////////////////////////////////////////////////////////////////////////
vector<shared_ptr<AssetEntry>>
   DerivationScheme_BIP32::extendPrivateChain(
   shared_ptr<DecryptedDataContainer> ddc,
   shared_ptr<AssetEntry> rootAsset, 
   unsigned start, unsigned end)
{
   //throws if the wallet is locked or the asset is missing its private key

   auto rootAsset_single = dynamic_pointer_cast<AssetEntry_Single>(rootAsset);
   if (rootAsset_single == nullptr)
      throw DerivationSchemeException("invalid root asset object");

   auto nextAsset = [this, ddc, rootAsset_single](
      unsigned derivationIndex)->shared_ptr<AssetEntry>
   {
      //sanity checks
      auto privkey = rootAsset_single->getPrivKey();
      if (privkey == nullptr)
         throw AssetUnavailableException();
      auto& privkeyData =
         ddc->getDecryptedPrivateData(privkey);

      auto& account_id = rootAsset_single->getAccountID();
      return computeNextPrivateEntry(
         ddc,
         privkeyData, move(privkey->getCipherDataPtr()->cipher_->getCopy()),
         account_id, derivationIndex);
   };

   if (ddc == nullptr)
      throw AssetUnavailableException();

   ReentrantLock lock(ddc.get());

   vector<shared_ptr<AssetEntry>> assetVec;

   for (unsigned i = start; i <= end; i++)
   {
      auto newAsset = nextAsset(i);
      assetVec.push_back(newAsset);
   }

   return assetVec;
}

////////////////////////////////////////////////////////////////////////////////
shared_ptr<AssetEntry_Single>
   DerivationScheme_BIP32::computeNextPublicEntry(
   const SecureBinaryData& pubKey,
   const BinaryData& accountID, unsigned index)
{
   //derScheme only allows for soft derivation
   if (index > 0x7FFFFFFF)
      throw DerivationSchemeException("illegal: hard derivation");

   BIP32_Node node;
   node.initFromPublicKey(depth_, leafId_, 0, pubKey, chainCode_);
   node.derivePublic(index);

   auto nextPubKey = node.movePublicKey();
   return make_shared<AssetEntry_Single>(
      index, accountID,
      nextPubKey, nullptr);
}

////////////////////////////////////////////////////////////////////////////////
vector<shared_ptr<AssetEntry>> DerivationScheme_BIP32::extendPublicChain(
   shared_ptr<AssetEntry> rootAsset,
   unsigned start, unsigned end)
{      
   auto rootSingle = dynamic_pointer_cast<AssetEntry_Single>(rootAsset);

   auto nextAsset = [this, rootSingle](
      unsigned derivationIndex)->shared_ptr<AssetEntry>
   {
      //get pubkey
      auto pubkey = rootSingle->getPubKey();
      auto& pubkeyData = pubkey->getCompressedKey();

      return computeNextPublicEntry(pubkeyData,
         rootSingle->getAccountID(), derivationIndex);
   };

   vector<shared_ptr<AssetEntry>> assetVec;

   for (unsigned i = start; i <= end; i++)
   {
      auto newAsset = nextAsset(i);
      assetVec.push_back(newAsset);
   }

   return assetVec;
}

////////////////////////////////////////////////////////////////////////////////
BinaryData DerivationScheme_BIP32::serialize() const
{
   BinaryWriter bw;
   bw.put_uint8_t(DERIVATIONSCHEME_BIP32);
   bw.put_var_int(chainCode_.getSize());
   bw.put_BinaryData(chainCode_);
   bw.put_uint32_t(depth_);
   bw.put_uint32_t(leafId_);

   BinaryWriter final;
   final.put_var_int(bw.getSize());
   final.put_BinaryData(bw.getData());

   return final.getData();
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
//// DerivationScheme_BIP32_Salted
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
std::shared_ptr<AssetEntry_Single> 
DerivationScheme_BIP32_Salted::computeNextPrivateEntry(
   std::shared_ptr<DecryptedDataContainer> ddc,
   const SecureBinaryData& privKey, std::unique_ptr<Cipher> cipher,
   const BinaryData& full_id, unsigned index)
{
   //derScheme only allows for soft derivation
   if (index > 0x7FFFFFFF)
      throw DerivationSchemeException("illegal: hard derivation");

   BIP32_Node node;
   node.initFromPrivateKey(
      getDepth(), getLeafId(), 0, privKey, getChaincode());
   node.derivePrivate(index);

   //salt the key
   auto&& saltedPrivKey = CryptoECDSA::PrivKeyScalarMultiply(
      node.getPrivateKey(), salt_);

   //compute salted pubkey
   auto&& saltedPubKey = CryptoECDSA().ComputePublicKey(saltedPrivKey, true);

   //encrypt the new privkey
   auto&& newCipher = cipher->getCopy(); //copying a cypher cycles the IV
   auto&& encryptedNextPrivKey = ddc->encryptData(
      newCipher.get(), saltedPrivKey);

   //instantiate encrypted salted privkey object
   auto privKeyID = full_id;
   privKeyID.append(WRITE_UINT32_BE(index));
   auto nextPrivKey = make_shared<Asset_PrivateKey>(
      privKeyID, encryptedNextPrivKey, move(newCipher));

   //instantiate and return new asset entry
   auto assetptr = make_shared<AssetEntry_Single>(
      index, full_id, saltedPubKey, nextPrivKey);

   return assetptr;
}

////////////////////////////////////////////////////////////////////////////////
std::shared_ptr<AssetEntry_Single> 
DerivationScheme_BIP32_Salted::computeNextPublicEntry(
   const SecureBinaryData& pubKey,
   const BinaryData& full_id, unsigned index)
{
   //derScheme only allows for soft derivation
   if (index > 0x7FFFFFFF)
      throw DerivationSchemeException("illegal: hard derivation");

   //compute pub key
   BIP32_Node node;
   node.initFromPublicKey(getDepth(), getLeafId(), 0, pubKey, getChaincode());
   node.derivePublic(index);
   auto nextPubkey = node.movePublicKey();

   //salt it
   auto&& saltedPubkey = CryptoECDSA::PubKeyScalarMultiply(nextPubkey, salt_);

   return make_shared<AssetEntry_Single>(
      index, full_id,
      saltedPubkey, nullptr);
}

////////////////////////////////////////////////////////////////////////////////
BinaryData DerivationScheme_BIP32_Salted::serialize() const
{
   BinaryWriter bw;
   bw.put_uint8_t(DERIVATIONSCHEME_BIP32_SALTED);
   bw.put_var_int(getChaincode().getSize());
   bw.put_BinaryData(getChaincode());
   bw.put_uint32_t(getDepth());
   bw.put_uint32_t(getLeafId());

   bw.put_var_int(salt_.getSize());
   bw.put_BinaryData(salt_);

   BinaryWriter final;
   final.put_var_int(bw.getSize());
   final.put_BinaryData(bw.getData());

   return final.getData();   
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
//// DerivationScheme_ECDH
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
DerivationScheme_ECDH::DerivationScheme_ECDH(const BinaryData& id,
   std::map<SecureBinaryData, unsigned> saltMap) :
   DerivationScheme(DerSchemeType_ECDH),
   id_(id), saltMap_(move(saltMap))
{
   set<unsigned> idSet;
   for (auto& saltPair : saltMap_)
   {
      auto insertIter = idSet.insert(saltPair.second);
      if (insertIter.second == false)
         throw DerivationSchemeException("ECDH id collision!");
   }

   if (idSet.size() == 0)
      return;

   auto idIter = idSet.rbegin();
   topSaltIndex_ = *idIter + 1;
}

////////////////////////////////////////////////////////////////////////////////
const SecureBinaryData& DerivationScheme_ECDH::getChaincode() const
{
   throw DerivationSchemeException("no chaincode for ECDH derivation scheme");
}

////////////////////////////////////////////////////////////////////////////////
BinaryData DerivationScheme_ECDH::serialize() const
{
   BinaryWriter bw;
   bw.put_uint8_t(DERIVATIONSCHEME_BIP32_ECDH);

   //id
   bw.put_var_int(id_.getSize());
   bw.put_BinaryData(id_);
   
   //length wrapper
   BinaryWriter final;
   final.put_var_int(bw.getSize());
   final.put_BinaryData(bw.getData());

   return final.getData();
}

////////////////////////////////////////////////////////////////////////////////
unsigned DerivationScheme_ECDH::addSalt(const SecureBinaryData& salt, LMDB* db)
{
   if (salt.getSize() != 32)
      throw DerivationSchemeException("salt is too small");

   //return the salt id if it's already in there
   auto saltIter = saltMap_.find(salt);
   if (saltIter != saltMap_.end())
      return saltIter->second;

   unique_lock<mutex> lock(saltMutex_);

   unsigned id = topSaltIndex_++;
   auto insertIter = saltMap_.insert(make_pair(salt, id));
   if (!insertIter.second)
      throw DerivationSchemeException("failed to insert salt");

   //update on disk
   putSalt(id, salt, db);

   //return insert index
   return id;
}

////////////////////////////////////////////////////////////////////////////////
void DerivationScheme_ECDH::putSalt(
   unsigned id, const SecureBinaryData& salt, LMDB* db)
{
   //update on disk
   BinaryWriter bwKey;
   bwKey.put_uint8_t(ECDH_SALT_PREFIX);
   bwKey.put_BinaryData(id_);
   bwKey.put_uint32_t(id, BE);

   BinaryWriter bwData;
   bwData.put_var_int(salt.getSize());
   bwData.put_BinaryData(salt);

   CharacterArrayRef carKey(bwKey.getSize(), bwKey.getDataRef().getPtr());
   CharacterArrayRef carData(bwData.getSize(), bwData.getDataRef().getPtr());
   db->insert(carKey, carData);
}

////////////////////////////////////////////////////////////////////////////////
void DerivationScheme_ECDH::putAllSalts(LMDB* db)
{
   //expects live read-write db tx
   for (auto& saltPair : saltMap_)
      putSalt(saltPair.second, saltPair.first, db);
}

////////////////////////////////////////////////////////////////////////////////
vector<shared_ptr<AssetEntry>> DerivationScheme_ECDH::extendPublicChain(
   shared_ptr<AssetEntry> root, unsigned start, unsigned end)
{
   auto rootSingle = dynamic_pointer_cast<AssetEntry_Single>(root);
   if (rootSingle == nullptr)
      throw DerivationSchemeException("unexpected root asset type");

   auto nextAsset = [this, rootSingle](
      unsigned derivationIndex)->shared_ptr<AssetEntry>
   {
      //get pubkey
      auto pubkey = rootSingle->getPubKey();
      auto& pubkeyData = pubkey->getCompressedKey();

      return computeNextPublicEntry(pubkeyData,
         rootSingle->getAccountID(), derivationIndex);
   };

   vector<shared_ptr<AssetEntry>> assetVec;

   for (unsigned i = start; i <= end; i++)
   {
      auto newAsset = nextAsset(i);
      assetVec.push_back(newAsset);
   }

   return assetVec;
}

////////////////////////////////////////////////////////////////////////////////
shared_ptr<AssetEntry_Single> DerivationScheme_ECDH::computeNextPublicEntry(
   const SecureBinaryData& pubKey,
   const BinaryData& full_id, unsigned index)
{
   if (pubKey.getSize() != 33)
      throw DerivationSchemeException("unexpected pubkey size");

   //get salt
   auto saltIter = saltMap_.rbegin();
   while (saltIter != saltMap_.rend())
   {
      if (saltIter->second == index)
         break;

      ++saltIter;
   }

   if (saltIter == saltMap_.rend())
      throw DerivationSchemeException("missing salt for id");

   if (saltIter->first.getSize() != 32)
      throw DerivationSchemeException("unexpected salt size");

   //salt root pubkey
   auto&& saltedPubkey = CryptoECDSA::PubKeyScalarMultiply(
      pubKey, saltIter->first);

   return make_shared<AssetEntry_Single>(
      index, full_id,
      saltedPubkey, nullptr);
}

////////////////////////////////////////////////////////////////////////////////
vector<shared_ptr<AssetEntry>> DerivationScheme_ECDH::extendPrivateChain(
   shared_ptr<DecryptedDataContainer> ddc,
   shared_ptr<AssetEntry> rootAsset, unsigned start, unsigned end)
{
   //throws if the wallet is locked or the asset is missing its private key

   auto rootAsset_single = dynamic_pointer_cast<AssetEntry_Single>(rootAsset);
   if (rootAsset_single == nullptr)
      throw DerivationSchemeException("invalid root asset object");

   auto nextAsset = [this, ddc, rootAsset_single](
      unsigned derivationIndex)->shared_ptr<AssetEntry>
   {
      //sanity checks
      auto privkey = rootAsset_single->getPrivKey();
      if (privkey == nullptr)
         throw AssetUnavailableException();
      auto& privkeyData =
         ddc->getDecryptedPrivateData(privkey);

      auto& account_id = rootAsset_single->getAccountID();
      return computeNextPrivateEntry(
         ddc,
         privkeyData, move(privkey->getCipherDataPtr()->cipher_->getCopy()),
         account_id, derivationIndex);
   };

   if (ddc == nullptr)
      throw AssetUnavailableException();

   ReentrantLock lock(ddc.get());

   vector<shared_ptr<AssetEntry>> assetVec;

   for (unsigned i = start; i <= end; i++)
   {
      auto newAsset = nextAsset(i);
      assetVec.push_back(newAsset);
   }

   return assetVec;
}

////////////////////////////////////////////////////////////////////////////////
shared_ptr<AssetEntry_Single>
DerivationScheme_ECDH::computeNextPrivateEntry(
   shared_ptr<DecryptedDataContainer> ddc,
   const SecureBinaryData& privKeyData, unique_ptr<Cipher> cipher,
   const BinaryData& accountID, unsigned index)
{
   //get salt
   auto saltIter = saltMap_.rbegin();
   while (saltIter != saltMap_.rend())
   {
      if (saltIter->second == index)
         break;

      ++saltIter;
   }

   if (saltIter == saltMap_.rend())
      throw DerivationSchemeException("missing salt for id");

   if (saltIter->first.getSize() != 32)
      throw DerivationSchemeException("unexpected salt size");

   //salt root privkey
   auto&& saltedPrivKey = CryptoECDSA::PrivKeyScalarMultiply(
      privKeyData, saltIter->first);

   //compute salted pubkey
   auto&& saltedPubKey = CryptoECDSA().ComputePublicKey(saltedPrivKey, true);

   //encrypt the new privkey
   auto&& newCipher = cipher->getCopy(); //copying a cypher cycles the IV
   auto&& encryptedNextPrivKey = ddc->encryptData(
      newCipher.get(), saltedPrivKey);

   //instantiate new encrypted key object
   auto privKeyID = accountID;
   privKeyID.append(WRITE_UINT32_BE(index));
   auto nextPrivKey = make_shared<Asset_PrivateKey>(
      privKeyID, encryptedNextPrivKey, move(newCipher));

   //instantiate and return new asset entry
   return make_shared<AssetEntry_Single>(
      index, accountID, saltedPubKey, nextPrivKey);
}

////////////////////////////////////////////////////////////////////////////////
unsigned DerivationScheme_ECDH::getSaltIndex(const SecureBinaryData& salt)
{
   auto iter = saltMap_.find(salt);
   if (iter == saltMap_.end())
      throw DerivationSchemeException("missing salt");

   return iter->second;
}