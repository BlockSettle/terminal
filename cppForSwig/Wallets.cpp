////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//  Copyright (C) 2016, goatpig                                               //
//  Distributed under the MIT license                                         //
//  See LICENSE-MIT or https://opensource.org/licenses/MIT                    //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

#include "Wallets.h"
#include "BlockDataManagerConfig.h"
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
//// KeyDerivationFunction
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
KeyDerivationFunction::~KeyDerivationFunction()
{}

////////////////////////////////////////////////////////////////////////////////
BinaryData KeyDerivationFunction_Romix::computeID() const
{
   BinaryWriter bw;
   bw.put_BinaryData(salt_);
   bw.put_uint32_t(iterations_);
   bw.put_uint32_t(memTarget_);

   return BtcUtils::getHash256(bw.getData());
}

////////////////////////////////////////////////////////////////////////////////
BinaryData KeyDerivationFunction_Romix::initialize()
{
   KdfRomix kdf;
   kdf.computeKdfParams(0);
   iterations_ = kdf.getNumIterations();
   memTarget_ = kdf.getMemoryReqtBytes();
   return kdf.getSalt();
}

////////////////////////////////////////////////////////////////////////////////
SecureBinaryData KeyDerivationFunction_Romix::deriveKey(
   const SecureBinaryData& rawKey) const
{
   KdfRomix kdfObj(memTarget_, iterations_, salt_);
   return move(kdfObj.DeriveKey(rawKey));
}

////////////////////////////////////////////////////////////////////////////////
shared_ptr<KeyDerivationFunction> KeyDerivationFunction::deserialize(
   const BinaryDataRef& data)
{
   BinaryRefReader brr(data);

   //check size
   auto totalLen = brr.get_var_int();
   if (totalLen != brr.getSizeRemaining())
      throw runtime_error("invalid serialized kdf size");

   //return ptr
   shared_ptr<KeyDerivationFunction> kdfPtr = nullptr;

   //check prefix
   auto prefix = brr.get_uint16_t();

   switch (prefix)
   {
   case KDF_ROMIX_PREFIX:
   {
      //iterations
      auto iterations = brr.get_uint32_t();

      //memTarget
      auto memTarget = brr.get_uint32_t();

      //salt
      auto len = brr.get_var_int();
      SecureBinaryData salt(move(brr.get_BinaryData(len)));

      kdfPtr = make_shared<KeyDerivationFunction_Romix>(
         iterations, memTarget, salt);
      break;
   }

   default:
      throw runtime_error("unexpected kdf prefix");
   }

   return kdfPtr;
}

////////////////////////////////////////////////////////////////////////////////
BinaryData KeyDerivationFunction_Romix::serialize() const
{
   BinaryWriter bw;
   bw.put_uint16_t(KDF_ROMIX_PREFIX);
   bw.put_uint32_t(iterations_);
   bw.put_uint32_t(memTarget_);
   bw.put_var_int(salt_.getSize());
   bw.put_BinaryData(salt_);

   BinaryWriter finalBw;
   finalBw.put_var_int(bw.getSize());
   finalBw.put_BinaryDataRef(bw.getDataRef());

   return finalBw.getData();
}

////////////////////////////////////////////////////////////////////////////////
const BinaryData& KeyDerivationFunction_Romix::getId(void) const
{
   if (id_.getSize() == 0)
      id_ = move(computeID());
   return id_;
}

////////////////////////////////////////////////////////////////////////////////
bool KeyDerivationFunction_Romix::isSame(KeyDerivationFunction* const kdf) const
{
   auto kdfromix = dynamic_cast<KeyDerivationFunction_Romix*>(kdf);
   if (kdfromix == nullptr)
      return false;

   return iterations_ == kdfromix->iterations_ && 
          memTarget_ == kdfromix->memTarget_ &&
          salt_ == kdfromix->salt_;
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
//// DecryptedData
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
void DecryptedEncryptionKey::deriveKey(
   shared_ptr<KeyDerivationFunction> kdf)
{
   if (derivedKeys_.find(kdf->getId()) != derivedKeys_.end())
      return;

   auto&& derivedkey = kdf->deriveKey(rawKey_);
   auto&& keypair = make_pair(kdf->getId(), move(derivedkey));
   derivedKeys_.insert(move(keypair));
}

////////////////////////////////////////////////////////////////////////////////
unique_ptr<DecryptedEncryptionKey> 
   DecryptedEncryptionKey::copy() const
{
   auto key_copy = rawKey_;
   auto copy_ptr = make_unique<DecryptedEncryptionKey>(key_copy);

   copy_ptr->derivedKeys_ = derivedKeys_;

   return move(copy_ptr);
}

////////////////////////////////////////////////////////////////////////////////
BinaryData DecryptedEncryptionKey::getId(
   const BinaryData& kdfId) const
{
   const auto keyIter = derivedKeys_.find(kdfId);
   if (keyIter == derivedKeys_.end())
      throw runtime_error("couldn't find derivation for kdfid");

   return move(computeId(keyIter->second));
}

////////////////////////////////////////////////////////////////////////////////
BinaryData DecryptedEncryptionKey::computeId(
   const SecureBinaryData& key) const
{
   //treat value as scalar, get pubkey for it
   auto&& hashedKey = BtcUtils::hash256(key);
   auto&& pubkey = CryptoECDSA().ComputePublicKey(hashedKey);
   
   //HMAC the pubkey, get last 16 bytes as ID
   return BtcUtils::computeDataId(pubkey, HMAC_KEY_ENCRYPTIONKEYS);
}

////////////////////////////////////////////////////////////////////////////////
const SecureBinaryData& DecryptedEncryptionKey::getDerivedKey(
   const BinaryData& id) const
{
   auto iter = derivedKeys_.find(id);
   if (iter == derivedKeys_.end())
      throw runtime_error("invalid key");

   return iter->second;
}


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
//// EncryptedDataContainer
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
void DecryptedDataContainer::initAfterLock()
{
   auto&& decryptedDataInstance = make_unique<DecryptedData>();

   //copy default encryption key
   auto&& defaultEncryptionKeyCopy = defaultEncryptionKey_.copy();

   auto defaultKey = 
      make_unique<DecryptedEncryptionKey>(defaultEncryptionKeyCopy);
   decryptedDataInstance->encryptionKeys_.insert(make_pair(
      defaultEncryptionKeyId_, move(defaultKey)));

   lockedDecryptedData_ = move(decryptedDataInstance);
}

////////////////////////////////////////////////////////////////////////////////
void DecryptedDataContainer::cleanUpBeforeUnlock()
{
   otherLocks_.clear();
   lockedDecryptedData_.reset();
}

////////////////////////////////////////////////////////////////////////////////
void DecryptedDataContainer::lockOther(
   shared_ptr<DecryptedDataContainer> other)
{
   if (!ownsLock())
      throw DecryptedDataContainerException("unlocked/does not own lock");

   if (lockedDecryptedData_ == nullptr)
      throw DecryptedDataContainerException(
      "nullptr lock! how did we get this far?");

   otherLocks_.push_back(OtherLockedContainer(other));
}

////////////////////////////////////////////////////////////////////////////////
unique_ptr<DecryptedEncryptionKey> 
   DecryptedDataContainer::deriveEncryptionKey(
   unique_ptr<DecryptedEncryptionKey> decrKey, const BinaryData& kdfid) const
{
   //sanity check
   if (!ownsLock())
      throw DecryptedDataContainerException("unlocked/does not own lock");

   if (lockedDecryptedData_ == nullptr)
      throw DecryptedDataContainerException(
      "nullptr lock! how did we get this far?");

   //does the decryption key have this derivation?
   auto derivationIter = decrKey->derivedKeys_.find(kdfid);
   if (derivationIter == decrKey->derivedKeys_.end())
   {
      //look for the kdf
      auto kdfIter = kdfMap_.find(kdfid);
      if (kdfIter == kdfMap_.end() || kdfIter->second == nullptr)
         throw DecryptedDataContainerException("can't find kdf params for id");
      
      //derive the key, this will insert it into the container too
      decrKey->deriveKey(kdfIter->second);
   }

   return move(decrKey);
}

////////////////////////////////////////////////////////////////////////////////
const SecureBinaryData& DecryptedDataContainer::getDecryptedPrivateKey(
   shared_ptr<Asset_PrivateKey> dataPtr)
{
   //sanity check
   if (!ownsLock())
      throw DecryptedDataContainerException("unlocked/does not own lock");

   if (lockedDecryptedData_ == nullptr)
      throw DecryptedDataContainerException(
         "nullptr lock! how did we get this far?");

   auto insertDecryptedData = [this](unique_ptr<DecryptedPrivateKey> decrKey)->
      const SecureBinaryData&
   {
      //if decrKey is empty, all casts failed, throw
      if (decrKey == nullptr)
         throw DecryptedDataContainerException("unexpected dataPtr type");

      //make sure insertion succeeds
      lockedDecryptedData_->privateKeys_.erase(decrKey->getId());
      auto&& keypair = make_pair(decrKey->getId(), move(decrKey));
      auto&& insertionPair =
         lockedDecryptedData_->privateKeys_.insert(move(keypair));

      return insertionPair.first->second->getDataRef();
   };

   //look for already decrypted data
   auto dataIter = lockedDecryptedData_->privateKeys_.find(dataPtr->getId());
   if (dataIter != lockedDecryptedData_->privateKeys_.end())
      return dataIter->second->getDataRef();

   //no decrypted val entry, let's try to decrypt the data instead
   
   if (!dataPtr->hasData())
   {
      //missing encrypted data in container (most likely uncomputed private key)
      //throw back to caller, this object only deals with decryption
      throw EncryptedDataMissing();
   }

   //check cypher
   if (dataPtr->cypher_ == nullptr)
   {
      //null cypher, data is not encrypted, create entry and return it
      auto dataCopy = dataPtr->data_;
      auto&& decrKey = make_unique<DecryptedPrivateKey>(
         dataPtr->getId(), dataCopy);
      return insertDecryptedData(move(decrKey));
   }

   //we have a valid cypher, grab the encryption key
   unique_ptr<DecryptedEncryptionKey> decrKey;
   auto& encryptionKeyId = dataPtr->cypher_->getEncryptionKeyId();
   auto& kdfId = dataPtr->cypher_->getKdfId();
   
   populateEncryptionKey(encryptionKeyId, kdfId);

   auto decrKeyIter = 
      lockedDecryptedData_->encryptionKeys_.find(encryptionKeyId);
   if (decrKeyIter == lockedDecryptedData_->encryptionKeys_.end())
      throw DecryptedDataContainerException("could not get encryption key");

   auto derivationKeyIter = decrKeyIter->second->derivedKeys_.find(kdfId);
   if(derivationKeyIter == decrKeyIter->second->derivedKeys_.end())
      throw DecryptedDataContainerException("could not get derived encryption key");

   //decrypt data
   auto decryptedDataPtr = move(dataPtr->decrypt(derivationKeyIter->second));

   //sanity check
   if (decryptedDataPtr == nullptr)
      throw DecryptedDataContainerException("failed to decrypt data");

   //insert the newly decrypted data in the container and return
   return insertDecryptedData(move(decryptedDataPtr));
}

////////////////////////////////////////////////////////////////////////////////
void DecryptedDataContainer::populateEncryptionKey(
   const BinaryData& keyid, const BinaryData& kdfid)
{
   //sanity check
   if (!ownsLock())
      throw DecryptedDataContainerException("unlocked/does not own lock");

   if (lockedDecryptedData_ == nullptr)
      throw DecryptedDataContainerException(
      "nullptr lock! how did we get this far?");

   //lambda to insert keys back into the container
   auto insertDecryptedData = [&keyid, this](
      unique_ptr<DecryptedEncryptionKey> decrKey)->void
   {
      //if decrKey is empty, all casts failed, throw
      if (decrKey == nullptr)
         throw DecryptedDataContainerException(
            "tried to insert empty decryption key");

      //make sure insertion succeeds
      lockedDecryptedData_->encryptionKeys_.erase(keyid);
      auto&& keypair = make_pair(keyid, move(decrKey));
      auto&& insertionPair =
         lockedDecryptedData_->encryptionKeys_.insert(move(keypair));
   };

   //look for already decrypted data
   unique_ptr<DecryptedEncryptionKey> decryptedKey = nullptr;
   auto dataIter = lockedDecryptedData_->encryptionKeys_.find(keyid);
   if (dataIter != lockedDecryptedData_->encryptionKeys_.end())
      decryptedKey = move(dataIter->second);

   if (decryptedKey == nullptr)
   {
      //we don't have a decrypted key, let's look for it in the encrypted map
      auto encrKeyIter = encryptionKeyMap_.find(keyid);
      if (encrKeyIter != encryptionKeyMap_.end())
      {
         //sanity check
         auto encryptedKeyPtr = dynamic_pointer_cast<Asset_EncryptionKey>(
            encrKeyIter->second);
         if (encryptedKeyPtr == nullptr)
            throw DecryptedDataContainerException(
               "unexpected object for encryption key id");

         //found the encrypted key, need to decrypt it first
         populateEncryptionKey(
            encryptedKeyPtr->cypher_->getEncryptionKeyId(),
            encryptedKeyPtr->cypher_->getKdfId());

         //grab encryption key from map
         auto decrKeyIter =
            lockedDecryptedData_->encryptionKeys_.find(
               encryptedKeyPtr->cypher_->getEncryptionKeyId());
         if (decrKeyIter == lockedDecryptedData_->encryptionKeys_.end())
            throw DecryptedDataContainerException("failed to decrypt key");
         auto&& decryptionKey = move(decrKeyIter->second);

         //derive encryption key
         decryptionKey = move(
            deriveEncryptionKey(move(decryptionKey),
            encryptedKeyPtr->cypher_->getKdfId()));

         //decrypt encrypted key
         auto&& rawDecryptedKey = encryptedKeyPtr->cypher_->decrypt(
            decryptionKey->getDerivedKey(encryptedKeyPtr->cypher_->getKdfId()),
            encryptedKeyPtr->data_);

         decryptedKey = move(make_unique<DecryptedEncryptionKey>(
            rawDecryptedKey));

         //move decryption key back to container
         insertDecryptedData(move(decryptionKey));
      }
   }

   if (decryptedKey == nullptr)
   {
      //still no key, prompt the user
      decryptedKey = move(promptPassphrase(keyid, kdfid));
   }

   //apply kdf
   decryptedKey = move(deriveEncryptionKey(move(decryptedKey), kdfid));

   //insert into map
   insertDecryptedData(move(decryptedKey));
}

////////////////////////////////////////////////////////////////////////////////
SecureBinaryData DecryptedDataContainer::encryptData(
   Cypher* const cypher, const SecureBinaryData& data)
{
   //sanity check
   if (cypher == nullptr)
      throw DecryptedDataContainerException("null cypher");

   if (!ownsLock())
      throw DecryptedDataContainerException("unlocked/does not own lock");

   if (lockedDecryptedData_ == nullptr)
      throw DecryptedDataContainerException(
      "nullptr lock! how did we get this far?");

   populateEncryptionKey(cypher->getEncryptionKeyId(), cypher->getKdfId());
   auto keyIter = lockedDecryptedData_->encryptionKeys_.find(
      cypher->getEncryptionKeyId());
   auto& derivedKey = keyIter->second->getDerivedKey(cypher->getKdfId());

   return move(cypher->encrypt(derivedKey, data));
}

////////////////////////////////////////////////////////////////////////////////
unique_ptr<DecryptedEncryptionKey> DecryptedDataContainer::promptPassphrase(
   const BinaryData& keyId, const BinaryData& kdfId) const
{
   while (1)
   {
      auto&& passphrase = getPassphraseLambda_(keyId);

      if (passphrase.getSize() == 0)
         throw DecryptedDataContainerException("empty passphrase");

      auto keyPtr = make_unique<DecryptedEncryptionKey>(passphrase);
      keyPtr = move(deriveEncryptionKey(move(keyPtr), kdfId));

      if (keyId == keyPtr->getId(kdfId))
         return move(keyPtr);
   }

   return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
void DecryptedDataContainer::updateKeyOnDisk(
   const BinaryData& key, shared_ptr<Asset_EncryptedData> dataPtr)
{
   //serialize db key
   auto&& dbKey = WRITE_UINT8_BE(ENCRYPTIONKEY_PREFIX);
   dbKey.append(key);

   updateKeyOnDiskNoPrefix(dbKey, dataPtr);
}

////////////////////////////////////////////////////////////////////////////////
void DecryptedDataContainer::updateKeyOnDiskNoPrefix(
   const BinaryData& dbKey, shared_ptr<Asset_EncryptedData> dataPtr)
{
   /*caller needs to manage db tx*/

   //check if data is on disk already
   CharacterArrayRef keyRef(dbKey.getSize(), dbKey.getPtr());
   auto&& dataRef = dbPtr_->get_NoCopy(keyRef);

   if (dataRef.len != 0)
   {
      BinaryDataRef bdr((uint8_t*)dataRef.data, dataRef.len);
      //already have this key, is it the same data?
      auto onDiskData = Asset_EncryptedData::deserialize(bdr);

      //data has not changed, no need to commit
      if (onDiskData->isSame(dataPtr.get()))
         return;

      //data has changed, wipe the existing data
      deleteKeyFromDisk(dbKey);
   }

   auto&& serializedData = dataPtr->serialize();
   CharacterArrayRef dataRef_Put(
      serializedData.getSize(), serializedData.getPtr());
   dbPtr_->insert(keyRef, dataRef_Put);

}

////////////////////////////////////////////////////////////////////////////////
void DecryptedDataContainer::updateOnDisk()
{
   //wallet needs to create the db read/write tx

   //encryption keys
   for (auto& key : encryptionKeyMap_)
      updateKeyOnDisk(key.first, key.second);

   //kdf
   for (auto& key : kdfMap_)
   {
      //get db key
      auto&& dbKey = WRITE_UINT8_BE(KDF_PREFIX);
      dbKey.append(key.first);

      //fetch from db
      CharacterArrayRef keyRef(dbKey.getSize(), dbKey.getPtr());
      auto&& dataRef = dbPtr_->get_NoCopy(keyRef);

      if (dataRef.len != 0)
      {
         BinaryDataRef bdr((uint8_t*)dataRef.data, dataRef.len);
         //already have this key, is it the same data?
         auto onDiskData = KeyDerivationFunction::deserialize(bdr);

         //data has not changed, not commiting to disk
         if (onDiskData->isSame(key.second.get()))
            continue;

         //data has changed, wipe the existing data
         deleteKeyFromDisk(dbKey);
      }

      auto&& serializedData = key.second->serialize();
      CharacterArrayRef dataRef_Put(
         serializedData.getSize(), serializedData.getPtr());
      dbPtr_->insert(keyRef, dataRef_Put);
   }
}

////////////////////////////////////////////////////////////////////////////////
void DecryptedDataContainer::deleteKeyFromDisk(const BinaryData& key)
{
   /***
   This operation abuses the no copy read feature in lmdb. Since all data is
   mmap'd, a no copy read is a pointer to the data on disk. Therefor modifying 
   that data will result in a modification on disk.

   This is done under 3 conditions: 
   1) The decrypted data container is locked.
   2) The calling threads owns a ReadWrite transaction on the lmdb object
   3) There are no active ReadOnly transactions on the lmdb object

   1. is a no brainer, 2. guarantees the changes are flushed to disk once the 
   tx is released. RW tx are locked, therefor only one is active at any given 
   time, by LMDB design.
   
   3. is to guarantee there are no readers when the change takes place. Needs
   some LMDB C++ wrapper modifications to be able to check from the db object. 
   The condition should be enforced by the caller regardless.
   ***/

   //sanity checks
   if (!ownsLock())
      throw DecryptedDataContainerException("unlocked/does not own lock");

   //check db only has one RW tx
   /*if (!dbEnv_->isRWLockExclusive())
   {
   throw DecryptedDataContainerException(
   "need exclusive RW lock to delete entries");
   }

   //check we own the RW tx
   if (dbEnv_->ownsLock() != LMDB_RWLOCK)
   {
   throw DecryptedDataContainerException(
   "need exclusive RW lock to delete entries");
   }*/

   CharacterArrayRef keyRef(key.getSize(), key.getCharPtr());
   
   //check data exist son disk to begin with
   {
      auto dataRef = dbPtr_->get_NoCopy(keyRef);

      //data is empty, nothing to wipe
      if (dataRef.len == 0)
      {
         throw DecryptedDataContainerException(
            "tried to wipe non existent entry");
      }
   }

   //wipe it
   dbPtr_->wipe(keyRef);
}

////////////////////////////////////////////////////////////////////////////////
void DecryptedDataContainer::readFromDisk()
{
   {
      //encryption key and kdf entries
      auto dbIter = dbPtr_->begin();

      BinaryWriter bwEncrKey;
      bwEncrKey.put_uint8_t(ENCRYPTIONKEY_PREFIX);
      
      CharacterArrayRef keyRef(bwEncrKey.getSize(), bwEncrKey.getData().getPtr());

      dbIter.seek(keyRef, LMDB::Iterator::Seek_GE);

      while (dbIter.isValid())
      {
         auto iterkey = dbIter.key();
         auto itervalue = dbIter.value();

         if (iterkey.mv_size < 2)
            throw runtime_error("empty db key");

         if (itervalue.mv_size < 1)
            throw runtime_error("empty value");

         BinaryDataRef keyBDR((uint8_t*)iterkey.mv_data +1, iterkey.mv_size -1);
         BinaryDataRef valueBDR((uint8_t*)itervalue.mv_data, itervalue.mv_size);

         auto prefix = (uint8_t*)iterkey.mv_data;
         switch (*prefix)
         {
         case ENCRYPTIONKEY_PREFIX:
         {
            auto keyPtr = Asset_EncryptedData::deserialize(valueBDR);
            auto encrKeyPtr = dynamic_pointer_cast<Asset_EncryptionKey>(keyPtr);
            if (encrKeyPtr == nullptr)
               throw runtime_error("empty keyptr");

            addEncryptionKey(encrKeyPtr);

            break;
         }

         case KDF_PREFIX:
         {
            auto kdfPtr = KeyDerivationFunction::deserialize(valueBDR);
            if (keyBDR != kdfPtr->getId())
               throw runtime_error("kdf id mismatch");

            addKdf(kdfPtr);
            break;
         }
         }

         dbIter.advance();
      }
   }
}

////////////////////////////////////////////////////////////////////////////////
void DecryptedDataContainer::encryptEncryptionKey(
   const BinaryData& keyID,
   const SecureBinaryData& newPassphrase)
{
   /***
   Encrypts an encryption key with "newPassphrase". If the key is already
   encrypted, it will be changed.
   ***/

   //sanity check
   if (!ownsLock())
      throw DecryptedDataContainerException("unlocked/does not own lock");

   if (lockedDecryptedData_ == nullptr)
      throw DecryptedDataContainerException(
      "nullptr lock! how did we get this far?");

   auto keyIter = encryptionKeyMap_.find(keyID);
   if (keyIter == encryptionKeyMap_.end())
      throw DecryptedDataContainerException(
         "cannot change passphrase for unknown key");

   //decrypt master encryption key
   auto& kdfId = keyIter->second->cypher_->getKdfId();
   populateEncryptionKey(keyID, kdfId);

   //grab decrypted key
   auto decryptedKeyIter = lockedDecryptedData_->encryptionKeys_.find(keyID);
   if (decryptedKeyIter == lockedDecryptedData_->encryptionKeys_.end())
      throw DecryptedDataContainerException(
         "failed to decrypt key");

   auto& decryptedKey = decryptedKeyIter->second->getData();

   //grab kdf for key id computation
   auto masterKeyKdfId = keyIter->second->cypher_->getKdfId();
   auto kdfIter = kdfMap_.find(masterKeyKdfId);
   if (kdfIter == kdfMap_.end())
      throw DecryptedDataContainerException("failed to grab kdf");

   //copy passphrase cause the ctor will move the data in
   auto newPassphraseCopy = newPassphrase;

   //kdf the key to get its id
   auto newEncryptionKey = make_unique<DecryptedEncryptionKey>(newPassphraseCopy);
   newEncryptionKey->deriveKey(kdfIter->second);
   auto newKeyId = newEncryptionKey->getId(masterKeyKdfId);

   //create new cypher, pointing to the new key id
   auto newCypher = keyIter->second->cypher_->getCopy(newKeyId);

   //add new encryption key object to container
   lockedDecryptedData_->encryptionKeys_.insert(
      move(make_pair(newKeyId, move(newEncryptionKey))));

   //encrypt master key
   auto&& newEncryptedKey = encryptData(newCypher.get(), decryptedKey);

   //create new encrypted container
   auto keyIdCopy = keyID;
   auto newEncryptedKeyPtr = 
      make_shared<Asset_EncryptionKey>(keyIdCopy, newEncryptedKey, move(newCypher));

   //update
   keyIter->second = newEncryptedKeyPtr;

   auto&& temp_key = WRITE_UINT8_BE(ENCRYPTIONKEY_PREFIX_TEMP);
   temp_key.append(keyID);
   auto&& perm_key = WRITE_UINT8_BE(ENCRYPTIONKEY_PREFIX);
   perm_key.append(keyID);

   {
      //write new encrypted key as temp key within it's own transaction
      LMDBEnv::Transaction tempTx(dbEnv_, LMDB::ReadWrite);
      updateKeyOnDiskNoPrefix(temp_key, newEncryptedKeyPtr);
   }

   {
      LMDBEnv::Transaction permTx(dbEnv_, LMDB::ReadWrite);

      //wipe old key from disk
      deleteKeyFromDisk(perm_key);

      //write new key to disk
      updateKeyOnDiskNoPrefix(perm_key, newEncryptedKeyPtr);
   }
      
   {
      LMDBEnv::Transaction permTx(dbEnv_, LMDB::ReadWrite);

      //wipe temp entry
      deleteKeyFromDisk(temp_key);    
   }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
//// AssetWallet
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
AssetWallet::~AssetWallet()
{
   derScheme_.reset();

   if (db_ != nullptr)
   {
      db_->close();
      delete db_;
      db_ = nullptr;
   }

   addresses_.clear();
   assets_.clear();
}

////////////////////////////////////////////////////////////////////////////////
shared_ptr<AssetWallet_Single> AssetWallet_Single::
createFromPrivateRoot_Armory135(
   const string& folder,
   AddressEntryType defaultAddressType,
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
   
   //create kdf and master encryption key
   auto kdfPtr = make_shared<KeyDerivationFunction_Romix>();
   auto&& masterKeySBD = SecureBinaryData().GenerateRandom(32);
   DecryptedEncryptionKey masterEncryptionKey(masterKeySBD);
   masterEncryptionKey.deriveKey(kdfPtr);
   auto&& masterEncryptionKeyId = masterEncryptionKey.getId(kdfPtr->getId());

   auto cypher = make_unique<Cypher_AES>(kdfPtr->getId(), 
      masterEncryptionKeyId);
   
   auto walletPtr = initWalletDb(
      wltMetaPtr,
      kdfPtr,
      masterEncryptionKey,
      move(cypher),
      passphrase, 
      defaultAddressType, 
      privateRoot, lookup);

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
   AddressEntryType defaultAddressType,
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
   pathSS << folder << "/armory_" << masterIDStr << "_wallet.lmdb";
   auto dbenv = getEnvFromFile(pathSS.str(), 2);

   initWalletMetaDB(dbenv, masterIDStr);

   auto wltMetaPtr = make_shared<WalletMeta_Single>(dbenv);
   wltMetaPtr->parentID_ = masterID;

   auto walletPtr = initWalletDbFromPubRoot(
      wltMetaPtr,
      defaultAddressType,
      pubRoot, chainCode, 
      lookup);

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
   auto&& addrVec = derScheme->extendPublicChain(rootEntry, 1);
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
   AddressEntryType addressType,
   const SecureBinaryData& privateRoot,
   unsigned lookup)
{
   //chaincode
   auto&& chaincode = BtcUtils::computeChainCode_Armory135(privateRoot);
   auto derScheme = make_shared<DerivationScheme_ArmoryLegacy>(
      chaincode, nullptr);

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

   auto rootAssetEntry = make_shared<AssetEntry_Single>(-1,
      pubkey, rootAsset);

   //compute wallet ID if it is missing
   if (metaPtr->walletID_.getSize() == 0)
      metaPtr->walletID_ = move(computeWalletID(derScheme, rootAssetEntry));
   
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
   LMDBEnv::Transaction tx(walletPtr->dbEnv_.get(), LMDB::ReadWrite);
   walletPtr->putHeaderData(
      metaPtr->parentID_, metaPtr->walletID_, derScheme, addressType, 0);

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
   
   //init walletptr from file
   walletPtr->readFromFile();

   {
      //asset lookup
      if (lookup == UINT32_MAX)
         lookup = DERIVATION_LOOKUP;

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

      walletPtr->extendPrivateChain(rootAssetEntry, lookup);
      
      //set empty passphrase lambda for the good measure
      auto emptyLambda = [](const BinaryData&)->SecureBinaryData
      {
         return SecureBinaryData();
      };

      walletPtr->decryptedData_->setPassphrasePromptLambda(emptyLambda);
   }

   return walletPtr;
}

////////////////////////////////////////////////////////////////////////////////
shared_ptr<AssetWallet_Single> AssetWallet_Single::initWalletDbFromPubRoot(
   shared_ptr<WalletMeta> metaPtr,
   AddressEntryType addressType,
   SecureBinaryData& pubRoot,
   SecureBinaryData& chainCode,
   unsigned lookup)
{
   //derScheme
   auto derScheme = make_shared<DerivationScheme_ArmoryLegacy>(
      chainCode, nullptr);

   //create root AssetEntry
   auto rootAssetEntry = make_shared<AssetEntry_Single>(-1,
      pubRoot, nullptr);

   //compute wallet ID
   if (metaPtr->walletID_.getSize() == 0)
      metaPtr->walletID_ = move(computeWalletID(derScheme, rootAssetEntry));

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
   LMDBEnv::Transaction tx(walletPtr->dbEnv_.get(), LMDB::ReadWrite);
   walletPtr->putHeaderData(
      metaPtr->parentID_, metaPtr->walletID_, derScheme, addressType, 0);

   {
      //DecryptedDataContainer
      walletPtr->decryptedData_->updateOnDisk();
   }

   {
      //root asset
      BinaryWriter bwKey;
      bwKey.put_uint32_t(ROOTASSET_KEY);

      auto&& data = rootAssetEntry->serialize();

      walletPtr->putData(bwKey.getData(), data);
   }

   //init walletptr from file
   walletPtr->readFromFile();

   {
      //asset lookup
      auto topEntryPtr = rootAssetEntry;

      if (lookup == UINT32_MAX)
         lookup = DERIVATION_LOOKUP;

      walletPtr->extendPublicChain(rootAssetEntry, lookup);
   }

   return walletPtr;
}

////////////////////////////////////////////////////////////////////////////////
void AssetWallet_Single::putHeaderData(const BinaryData& parentID,
   const BinaryData& walletID,
   shared_ptr<DerivationScheme> derScheme,
   AddressEntryType aet, int topUsedIndex)
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

   AssetWallet::putHeaderData(parentID, walletID, derScheme, aet, topUsedIndex);
}

////////////////////////////////////////////////////////////////////////////////
shared_ptr<AssetWallet_Multisig> AssetWallet_Multisig::createFromPrivateRoot(
   const string& folder,
   AddressEntryType aet,
   unsigned M, unsigned N,
   SecureBinaryData& privateRoot,
   const SecureBinaryData& passphrase,
   unsigned lookup)
{
   if (aet != AddressEntryType_Nested_Multisig && 
       aet != AddressEntryType_P2WSH &&
       aet != AddressEntryType_Nested_P2WSH)
      throw WalletException("invalid AddressEntryType for MS wallet");

   //pub root
   auto&& pubkey = CryptoECDSA().ComputePublicKey(privateRoot);
   
   //compute master ID as hmac256(root pubkey, "MetaEntry")
   string hmacMasterMsg("MetaEntry");
   auto&& masterID_long = BtcUtils::getHMAC256(
      pubkey, SecureBinaryData(hmacMasterMsg));
   auto&& masterID = BtcUtils::computeID(masterID_long);
   string masterIDStr(masterID.getCharPtr(), masterID.getSize());

   //create wallet file
   stringstream pathSS;
   pathSS << folder << "/armory_" << masterIDStr << "_wallet.lmdb";
   
   //create dbenv: N subwallets, 1 top db, 1 meta db
   auto dbenv = getEnvFromFile(pathSS.str(), N + 2); 
   
   //create meta entry
   initWalletMetaDB(dbenv, masterIDStr);
   auto mainWltMetaPtr = make_shared<WalletMeta_Multisig>(dbenv);

   //compute wallet ID as hmac256(root pubkey, "M_of_N")
   stringstream mofn;
   mofn << M << "_of_" << N;
   auto&& longID = BtcUtils::getHMAC256(pubkey, SecureBinaryData(mofn.str()));
   auto&& walletID = BtcUtils::computeID(longID);
   
   mainWltMetaPtr->walletID_ = walletID;
   string walletIDStr(walletID.getCharPtr(), walletID.getSize());
   mainWltMetaPtr->dbName_ = walletIDStr;

   auto walletPtr = make_shared<AssetWallet_Multisig>(mainWltMetaPtr);

   LMDB dbMeta;
   {
      //put main name in meta db
      dbMeta.open(dbenv.get(), WALLETMETA_DBNAME);
   
      LMDBEnv::Transaction metatx(dbenv.get(), LMDB::ReadWrite);
      putDbName(&dbMeta, mainWltMetaPtr);
      setMainWallet(&dbMeta, mainWltMetaPtr);
   }

   auto kdfPtr = make_shared<KeyDerivationFunction_Romix>();
   auto&& masterKey_randomized = SecureBinaryData().GenerateRandom(32);
   DecryptedEncryptionKey masterEncryptionKey(masterKey_randomized);

   //create N sub wallets
   map<BinaryData, shared_ptr<AssetWallet_Single>> subWallets;

   for (unsigned i = 0; i < N; i++)
   {
      //get sub wallet root
      stringstream hmacMsg;
      hmacMsg << "Subwallet-" << i;

      SecureBinaryData subRoot(32);
      BtcUtils::getHMAC256(privateRoot.getPtr(), privateRoot.getSize(),
         hmacMsg.str().c_str(), hmacMsg.str().size(), subRoot.getPtr());

      auto subWalletMeta = make_shared<WalletMeta_Single>(dbenv);
      subWalletMeta->parentID_ = walletID;
      subWalletMeta->dbName_ = hmacMsg.str();

      masterEncryptionKey.deriveKey(kdfPtr);
      auto&& masterEncryptionKeyId = masterEncryptionKey.getId(kdfPtr->getId());
      auto cypher = make_unique<Cypher_AES>(kdfPtr->getId(),
         masterEncryptionKeyId);

      auto subWalletPtr = AssetWallet_Single::initWalletDb(
         subWalletMeta, 
         kdfPtr,
         masterEncryptionKey,
         move(cypher),
         passphrase,
         AddressEntryType_P2PKH, 
         move(subRoot), lookup);

      subWallets[subWalletPtr->getID()] = subWalletPtr;
   }

   //create derScheme
   auto derScheme = make_shared<DerivationScheme_Multisig>(
      subWallets, N, M);

   {
      LMDBEnv::Transaction tx(walletPtr->dbEnv_.get(), LMDB::ReadWrite);

      {
         //wallet type
         BinaryWriter bwKey;
         bwKey.put_uint32_t(WALLETTYPE_KEY);

         BinaryWriter bwData;
         bwData.put_var_int(4);
         bwData.put_uint32_t(WalletMetaType_Multisig);

         walletPtr->putData(bwKey, bwData);
      }

      //header
      walletPtr->putHeaderData(
         masterID, walletID, derScheme, aet, 0);

      {
         //chainlength
         BinaryWriter bwKey;
         bwKey.put_uint8_t(ASSETENTRY_PREFIX);

         BinaryWriter bwData;
         bwData.put_var_int(4);
         bwData.put_uint32_t(lookup);

         walletPtr->putData(bwKey, bwData);
      }
   }

   //clean subwallets ptr and derScheme
   derScheme.reset();
   subWallets.clear();

   //load from db
   walletPtr->readFromFile();

   return walletPtr;
}

////////////////////////////////////////////////////////////////////////////////
void AssetWallet::putHeaderData(const BinaryData& parentID,
   const BinaryData& walletID, 
   shared_ptr<DerivationScheme> derScheme,
   AddressEntryType aet, int topUsedIndex)
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

   {
      //derivation scheme
      BinaryWriter bwKey;
      bwKey.put_uint32_t(DERIVATIONSCHEME_KEY);

      auto&& data = derScheme->serialize();
      putData(bwKey.getData(), data);
   }

   {
      //default AddressEntryType
      BinaryWriter bwKey;
      bwKey.put_uint32_t(ADDRESSENTRYTYPE_KEY);

      BinaryWriter bwData;
      bwData.put_var_int(1);
      bwData.put_uint8_t(aet);

      putData(bwKey, bwData);
   }

   {
      //top used index
      BinaryWriter bwKey;
      bwKey.put_uint32_t(TOPUSEDINDEX_KEY);

      BinaryWriter bwData;
      bwData.put_var_int(4);
      bwData.put_int32_t(topUsedIndex);

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

   BinaryRefReader brr((const uint8_t*)ref.data, ref.len);
   auto len = brr.get_var_int();
   if (len != brr.getSizeRemaining())
      throw WalletException("on disk data length mismatch");

   return brr.get_BinaryDataRef(brr.getSizeRemaining());
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
      //derivation scheme
      BinaryWriter bwKey;
      bwKey.put_uint32_t(DERIVATIONSCHEME_KEY);
      auto derSchemeRef = getDataRefForKey(bwKey.getData());

      derScheme_ = DerivationScheme::deserialize(derSchemeRef);
      auto derSchemeArmoryLegacy = 
         dynamic_pointer_cast<DerivationScheme_ArmoryLegacy>(derScheme_);
      derSchemeArmoryLegacy->setDecryptedDataContainerPtr(decryptedData_);
   }

   {
      //default AddressEntryType
      BinaryWriter bwKey;
      bwKey.put_uint32_t(ADDRESSENTRYTYPE_KEY);
      auto defaultAetRef = getDataRefForKey(bwKey.getData());

      if (defaultAetRef.getSize() != 1)
         throw WalletException("invalid aet length");

      default_aet_ = (AddressEntryType)*defaultAetRef.getPtr();
   }

   {
      //top used index
      BinaryWriter bwKey;
      bwKey.put_uint32_t(TOPUSEDINDEX_KEY);
      auto topIndexRef = getDataRefForKey(bwKey.getData());

      if (topIndexRef.getSize() != 4)
         throw WalletException("invalid topindex length");

      BinaryRefReader brr(topIndexRef);
      highestUsedAddressIndex_.store(brr.get_int32_t(), memory_order_relaxed);
   }

   {
      //root asset
      BinaryWriter bwKey;
      bwKey.put_uint32_t(ROOTASSET_KEY);
      auto rootAssetRef = getDataRefForKey(bwKey.getData());

      root_ = AssetEntry::deserDBValue(-1, rootAssetRef);
   }

   //encryption keys and kdfs
   decryptedData_->readFromDisk();

   {
      //asset entries
      auto dbIter = db_->begin();

      BinaryWriter bwKey;
      bwKey.put_uint8_t(ASSETENTRY_PREFIX);
      CharacterArrayRef keyRef(bwKey.getSize(), bwKey.getData().getPtr());

      dbIter.seek(keyRef, LMDB::Iterator::Seek_GE);

      while (dbIter.isValid())
      {
         auto iterkey = dbIter.key();
         auto itervalue = dbIter.value();

         BinaryDataRef keyBDR((uint8_t*)iterkey.mv_data, iterkey.mv_size);
         if (!keyBDR.startsWith(bwKey.getDataRef()))
         {
            dbIter.advance();
            continue;
         }

         BinaryDataRef valueBDR((uint8_t*)itervalue.mv_data, itervalue.mv_size);

         //check value's advertized size is packet size and strip it
         BinaryRefReader brrVal(valueBDR);
         auto valsize = brrVal.get_var_int();
         if (valsize != brrVal.getSizeRemaining())
            throw WalletException("entry val size mismatch");
         
         try
         {
            auto entryPtr = AssetEntry::deserialize(keyBDR, 
               brrVal.get_BinaryDataRef(brrVal.getSizeRemaining()));
            assets_.insert(make_pair(entryPtr->getId(), entryPtr));
         }
         catch (AssetDeserException& e)
         {
            LOGERR << e.what();
            break;
         }

         dbIter.advance();
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
         //default AddressEntryType
         BinaryWriter bwKey;
         bwKey.put_uint32_t(ADDRESSENTRYTYPE_KEY);
         auto defaultAetRef = getDataRefForKey(bwKey.getData());

         if (defaultAetRef.getSize() != 1)
            throw WalletException("invalid aet length");

         default_aet_ = (AddressEntryType)*defaultAetRef.getPtr();
      }

      {
         //top used index
         BinaryWriter bwKey;
         bwKey.put_uint32_t(TOPUSEDINDEX_KEY);
         auto topIndexRef = getDataRefForKey(bwKey.getData());

         if (topIndexRef.getSize() != 4)
            throw WalletException("invalid topindex length");

         BinaryRefReader brr(topIndexRef);
         highestUsedAddressIndex_.store(brr.get_int32_t(), memory_order_relaxed);
      }

      {
         //derivation scheme
         BinaryWriter bwKey;
         bwKey.put_uint32_t(DERIVATIONSCHEME_KEY);
         auto derSchemeRef = getDataRefForKey(bwKey.getData());

         derScheme_ = DerivationScheme::deserialize(derSchemeRef);
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
      //sub wallets
      auto derSchemeMS =
         dynamic_pointer_cast<DerivationScheme_Multisig>(derScheme_);

      if (derSchemeMS == nullptr)
         throw WalletException("unexpected derScheme ptr type");

      auto n = derSchemeMS->getN();

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

      derSchemeMS->setSubwalletPointers(walletPtrs);
   }

   {
      auto derSchemeMS = dynamic_pointer_cast<DerivationScheme_Multisig>(derScheme_);
      if (derSchemeMS == nullptr)
         throw WalletException("unexpected derScheme type");

      //build AssetEntry map
      for (unsigned i = 0; i < chainLength_; i++)
         assets_[i] = derSchemeMS->getAssetForIndex(i);
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
unsigned AssetWallet::getAndBumpHighestUsedIndex()
{
   LMDBEnv::Transaction tx(dbEnv_.get(), LMDB::ReadWrite);

   auto index = highestUsedAddressIndex_.fetch_add(1, memory_order_relaxed);

   BinaryWriter bwKey;
   bwKey.put_uint32_t(TOPUSEDINDEX_KEY);

   BinaryWriter bwData;
   bwData.put_var_int(4);
   bwData.put_int32_t(highestUsedAddressIndex_.load(memory_order_relaxed));

   putData(bwKey, bwData);

   return index;
}

////////////////////////////////////////////////////////////////////////////////
shared_ptr<AddressEntry> AssetWallet::getNewAddress()
{
   //increment top used address counter & update
   auto index = getAndBumpHighestUsedIndex();

   //lock
   ReentrantLock lock(this);

   auto addrIter = addresses_.find(index);
   if (addrIter != addresses_.end())
      return addrIter->second;

   //check look up
   auto entryIter = assets_.find(index);
   if (entryIter == assets_.end())
   {
      if (assets_.size() == 0)
         throw WalletException("uninitialized wallet");
      extendPublicChain(DERIVATION_LOOKUP);

      entryIter = assets_.find(index);
      if (entryIter == assets_.end())
         throw WalletException("requested index overflows max lookup");
   }
   
   auto aePtr = getAddressEntryForAsset(entryIter->second, default_aet_);

   //insert new entry
   addresses_[aePtr->getIndex()] = aePtr;

   return aePtr;
}

////////////////////////////////////////////////////////////////////////////////
bool AssetWallet::hasScrAddr(const BinaryData& scrAddr)
{
   return getAssetIndexForAddr(scrAddr) != INT32_MAX;
}

////////////////////////////////////////////////////////////////////////////////
int AssetWallet::getAssetIndexForAddr(const BinaryData& scrAddr)
{
   auto getIndexForAddr = [&](BinaryDataRef scriptHash)->int
   {
      auto prefix = scriptHash.getPtr();
      auto hashRef = scriptHash.getSliceRef(1, scriptHash.getSize() - 1);

      switch (*prefix)
      {
      case SCRIPT_PREFIX_HASH160:
      case SCRIPT_PREFIX_HASH160_TESTNET:
      {
         auto iter = hashMaps_.hashCompressed_.find(hashRef);
         if (iter != hashMaps_.hashCompressed_.end())
            return iter->second;

         auto iter2 = hashMaps_.hashUncompressed_.find(hashRef);
         if (iter2 != hashMaps_.hashUncompressed_.end())
            return iter2->second;

         break;
      }

        
      case SCRIPT_PREFIX_P2SH:
      case SCRIPT_PREFIX_P2SH_TESTNET:
      {
         auto iter1 = hashMaps_.hashNestedP2PK_.find(hashRef);
         if (iter1 != hashMaps_.hashNestedP2PK_.end())
            return iter1->second;
         
         auto iter2 = hashMaps_.hashNestedP2WPKH_.find(hashRef);
         if (iter2 != hashMaps_.hashNestedP2WPKH_.end())
            return iter2->second;

         auto iter3 = hashMaps_.hashNestedMultisig_.find(hashRef);
         if (iter3 != hashMaps_.hashNestedMultisig_.end())
            return iter3->second;

         auto iter4 = hashMaps_.hashNestedP2WSH_.find(hashRef);
         if (iter4 != hashMaps_.hashNestedP2WSH_.end())
            return iter4->second;

         break;
      }

      default:
         throw runtime_error("invalid script hash prefix");
      }

      return INT32_MAX;
   };

   auto getIndexForAddrNoPrefix = [&](BinaryDataRef scriptHash)->int
   {
      auto iter = hashMaps_.hashCompressed_.find(scriptHash);
      if (iter != hashMaps_.hashCompressed_.end())
         return iter->second;

      auto iter2 = hashMaps_.hashUncompressed_.find(scriptHash);
      if (iter2 != hashMaps_.hashUncompressed_.end())
         return iter2->second;

      auto iter3 = hashMaps_.hashNestedP2PK_.find(scriptHash);
      if (iter3 != hashMaps_.hashNestedP2PK_.end())
         return iter3->second;

      auto iter4 = hashMaps_.hashNestedP2WPKH_.find(scriptHash);
      if (iter4 != hashMaps_.hashNestedP2WPKH_.end())
         return iter4->second;

      auto iter5 = hashMaps_.hashNestedMultisig_.find(scriptHash);
      if (iter5 != hashMaps_.hashNestedMultisig_.end())
         return iter5->second;

      auto iter6 = hashMaps_.hashNestedP2WSH_.find(scriptHash);
      if (iter6 != hashMaps_.hashNestedP2WSH_.end())
         return iter6->second;

      return INT32_MAX;
   };

   ReentrantLock lock(this);

   fillHashIndexMap();

   if (scrAddr.getSize() == 21)
   {
      try
      {
         return getIndexForAddr(scrAddr.getRef());
      }
      catch (...)
      {
      }
   }
   else if (scrAddr.getSize() == 20)
   {
      return getIndexForAddrNoPrefix(scrAddr.getRef());
   }

   auto&& scriptHash = BtcUtils::base58toScriptAddr(scrAddr);
   return getIndexForAddr(scriptHash);
}

////////////////////////////////////////////////////////////////////////////////
AddressEntryType AssetWallet::getAddrTypeForIndex(int index)
{
   ReentrantLock lock(this);
   AddressEntryType addrType;
   
   auto addrIter = addresses_.find(index);
   if (addrIter != addresses_.end())
      addrType = addrIter->second->getType();

   auto assetIter = assets_.find(index);
   if (assetIter == assets_.end())
      throw WalletException("invalid index");

   addrType = assetIter->second->getAddrType();

   if (addrType == AddressEntryType_Default)
      addrType = default_aet_;
   return addrType;
}

////////////////////////////////////////////////////////////////////////////////
shared_ptr<AddressEntry> AssetWallet_Single::getAddressEntryForAsset(
   shared_ptr<AssetEntry> assetPtr, AddressEntryType ae_type)
{
   ReentrantLock lock(this);
   
   if (ae_type == AddressEntryType_Default)
      ae_type = default_aet_;

   auto prev_aet = assetPtr->getAddrType();

   auto addrIter = addresses_.find(assetPtr->getId());
   if (addrIter != addresses_.end())
   {
      if(addrIter->second->getType() == ae_type)
         return addrIter->second;
   }

   shared_ptr<AddressEntry> aePtr = nullptr;
   switch (ae_type)
   {
   case AddressEntryType_P2PKH:
      aePtr = make_shared<AddressEntry_P2PKH>(assetPtr);
      break;

   case AddressEntryType_P2WPKH:
      aePtr = make_shared<AddressEntry_P2WPKH>(assetPtr);
      break;

   case AddressEntryType_Nested_P2WPKH:
      aePtr = make_shared<AddressEntry_Nested_P2WPKH>(assetPtr);
      break;

   case AddressEntryType_Nested_P2PK:
      aePtr = make_shared<AddressEntry_Nested_P2PK>(assetPtr);
      break;

   default:
      throw WalletException("unsupported address entry type");
   }

   if (ae_type == prev_aet)
      assetPtr->doNotCommit();
   else
      writeAssetEntry(assetPtr);

   addresses_[assetPtr->getId()] = aePtr;
   return aePtr;
}

////////////////////////////////////////////////////////////////////////////////
shared_ptr<AddressEntry> AssetWallet_Multisig::getAddressEntryForAsset(
   shared_ptr<AssetEntry> assetPtr, AddressEntryType ae_type)
{
   ReentrantLock lock(this);

   auto addrIter = addresses_.find(assetPtr->getId());
   if (addrIter != addresses_.end())
      return addrIter->second;

   shared_ptr<AddressEntry> aePtr = nullptr;
   switch (ae_type)
   {
   case AddressEntryType_Nested_Multisig:
      aePtr = make_shared<AddressEntry_Nested_Multisig>(assetPtr);
      break;

   case AddressEntryType_P2WSH:
      aePtr = make_shared<AddressEntry_P2WSH>(assetPtr);
      break;

   case AddressEntryType_Nested_P2WSH:
      aePtr = make_shared<AddressEntry_Nested_P2WSH>(assetPtr);
      break;

   default:
      throw WalletException("unsupported address entry type");
   }

   addresses_.insert(make_pair(assetPtr->getId(), aePtr));
   return aePtr;
}

////////////////////////////////////////////////////////////////////////////////
shared_ptr<AddressEntry> AssetWallet::getAddressEntryForIndex(int index)
{
   ReentrantLock lock(this);

   auto addrIter = addresses_.find(index);
   if (addrIter != addresses_.end())
      return addrIter->second;

   auto asset = getAssetForIndex(index);
   return getAddressEntryForAsset(asset, asset->getAddrType());
}

////////////////////////////////////////////////////////////////////////////////
void AssetWallet::writeAssetEntry(shared_ptr<AssetEntry> entryPtr)
{
   if (!entryPtr->needsCommit())
      return;

   auto&& serializedEntry = entryPtr->serialize();
   auto&& dbKey = entryPtr->getDbKey();

   CharacterArrayRef keyRef(dbKey.getSize(), dbKey.getPtr());
   CharacterArrayRef dataRef(serializedEntry.getSize(), serializedEntry.getPtr());

   LMDBEnv::Transaction tx(dbEnv_.get(), LMDB::ReadWrite);
   db_->insert(keyRef, dataRef);

   entryPtr->doNotCommit();
}

////////////////////////////////////////////////////////////////////////////////
void AssetWallet::deleteAssetEntry(shared_ptr<AssetEntry> entryPtr)
{
   auto&& dbKey = entryPtr->getDbKey();
   CharacterArrayRef keyRef(dbKey.getSize(), dbKey.getPtr());

   LMDBEnv::Transaction tx(dbEnv_.get(), LMDB::ReadWrite);
   db_->erase(keyRef);
}


////////////////////////////////////////////////////////////////////////////////
void AssetWallet::updateOnDiskAssets()
{
   LMDBEnv::Transaction tx(dbEnv_.get(), LMDB::ReadWrite);

   for (auto& entryPtr : assets_)
      writeAssetEntry(entryPtr.second);
}

////////////////////////////////////////////////////////////////////////////////
void AssetWallet::deleteImports(const vector<BinaryData>& addrVec)
{
   ReentrantLock lock(this);

   for (auto& scrAddr : addrVec)
   {
      int importIndex = INT32_MAX;
      try
      {
         //if import index does not exist or isnt negative, continue
         //only imports use a negative derivation index
         importIndex = getAssetIndexForAddr(scrAddr);
         if (importIndex > 0 || importIndex == INT32_MAX)
            continue;
      }
      catch (...)
      {
         continue;
      }

      auto assetIter = assets_.find(importIndex);
      if (assetIter == assets_.end())
         continue;

      auto assetPtr = assetIter->second;

      //remove from wallet's maps
      assets_.erase(importIndex);
      addresses_.erase(importIndex);

      //erase from file
      deleteAssetEntry(assetPtr);
   }
}

////////////////////////////////////////////////////////////////////////////////
const string& AssetWallet::getFilename() const
{
   if (dbEnv_ == nullptr)
      throw runtime_error("null dbenv");

   return dbEnv_->getFilename();
}

////////////////////////////////////////////////////////////////////////////////
void AssetWallet_Single::fillHashIndexMap()
{
   ReentrantLock lock(this);

   if ((assets_.size() > 0 && lastKnownIndex_ != assets_.rbegin()->first) ||
      lastAssetMapSize_ != assets_.size())
   {
      hashMaps_.clear();

      for (auto& entry : assets_)
      {
         auto assetSingle = dynamic_pointer_cast<AssetEntry_Single>(entry.second);
         auto&& hashMap = assetSingle->getScriptHashMap();
         
         hashMaps_.hashUncompressed_.insert(make_pair(
            hashMap[ScriptHash_P2PKH_Uncompressed], assetSingle->getId()));
         
         hashMaps_.hashCompressed_.insert(make_pair(
            hashMap[ScriptHash_P2PKH_Compressed], assetSingle->getId()));
         
         hashMaps_.hashNestedP2WPKH_.insert(make_pair(
            hashMap[ScriptHash_P2WPKH], assetSingle->getId()));

         hashMaps_.hashNestedP2PK_.insert(make_pair(
            hashMap[ScriptHash_Nested_P2PK], assetSingle->getId()));
      }

      lastKnownIndex_ = assets_.rbegin()->first;
      lastAssetMapSize_ = assets_.size();
   }
}

////////////////////////////////////////////////////////////////////////////////
set<BinaryData> AssetWallet_Single::getAddrHashSet()
{
   ReentrantLock lock(this);

   fillHashIndexMap();

   set<BinaryData> addrHashSet;
   uint8_t prefix = BlockDataManagerConfig::getPubkeyHashPrefix();

   for (auto& hashIndexPair : hashMaps_.hashUncompressed_)
   {
      BinaryWriter bw;
      bw.put_uint8_t(prefix);
      bw.put_BinaryDataRef(hashIndexPair.first);
      addrHashSet.insert(bw.getData());
   }

   prefix = BlockDataManagerConfig::getScriptHashPrefix();

   for (auto& hashIndexPair : hashMaps_.hashNestedP2WPKH_)
   {
      BinaryWriter bw;
      bw.put_uint8_t(prefix);
      bw.put_BinaryDataRef(hashIndexPair.first);
      addrHashSet.insert(bw.getData());
   }

   for (auto& hashIndexPair : hashMaps_.hashNestedP2PK_)
   {
      BinaryWriter bw;
      bw.put_uint8_t(prefix);
      bw.put_BinaryDataRef(hashIndexPair.first);
      addrHashSet.insert(bw.getData());
   }

   return addrHashSet;
}

////////////////////////////////////////////////////////////////////////////////
const SecureBinaryData& AssetWallet_Single::getPublicRoot() const
{
   auto rootEntry = dynamic_pointer_cast<AssetEntry_Single>(root_);
   auto pubEntry = rootEntry->getPubKey();

   return pubEntry->getUncompressedKey();
}

////////////////////////////////////////////////////////////////////////////////
const SecureBinaryData& AssetWallet_Single::getChainCode() const
{
   auto derSchemeA135 =
      dynamic_pointer_cast<DerivationScheme_ArmoryLegacy>(derScheme_);

   if (derSchemeA135 == nullptr)
      throw runtime_error("unexpected derivation scheme for AssetWallet_Single");

   return derSchemeA135->getChainCode();
}

////////////////////////////////////////////////////////////////////////////////
void AssetWallet_Multisig::fillHashIndexMap()
{
   ReentrantLock lock(this);

   if ((assets_.size() > 0 && lastKnownIndex_ != assets_.rbegin()->first) ||
      lastAssetMapSize_ != assets_.size())
   {
      hashMaps_.clear();

      switch (default_aet_)
      {
      case AddressEntryType_Nested_P2WSH:
      {
         for (auto& entry : assets_)
         {
            auto assetMS = dynamic_pointer_cast<AssetEntry_Multisig>(entry.second);
            hashMaps_.hashNestedP2WSH_.insert(
               make_pair(assetMS->getP2WSHScriptH160().getRef(), assetMS->getId()));
         }

         break;
      }

      case AddressEntryType_P2WSH:
      {
         for (auto& entry : assets_)
         {
            auto assetMS = dynamic_pointer_cast<AssetEntry_Multisig>(entry.second);
            hashMaps_.hashP2WSH_.insert(
               make_pair(assetMS->getHash256().getRef(), assetMS->getId()));
         }

         break;
      }

      case AddressEntryType_Nested_Multisig:
      {
         for (auto& entry : assets_)
         {
            auto assetMS = dynamic_pointer_cast<AssetEntry_Multisig>(entry.second);
            hashMaps_.hashNestedMultisig_.insert(
               make_pair(assetMS->getHash160().getRef(), assetMS->getId()));
         }

         break;
      }

      default:
         throw WalletException("unexpected AddressEntryType for MS wallet");
      }

      lastKnownIndex_ = assets_.rbegin()->first;
      lastAssetMapSize_ = assets_.size();
   }
}

////////////////////////////////////////////////////////////////////////////////
set<BinaryData> AssetWallet_Multisig::getAddrHashSet()
{
   ReentrantLock lock(this);

   fillHashIndexMap();

   set<BinaryData> addrHashSet;
   uint8_t prefix = BlockDataManagerConfig::getScriptHashPrefix();
   
   for (auto& hashIndexPair : hashMaps_.hashNestedMultisig_)
   {
      BinaryWriter bw;
      bw.put_uint8_t(prefix);
      bw.put_BinaryDataRef(hashIndexPair.first);
      addrHashSet.insert(bw.getData());
   }

   for (auto& hashIndexPair : hashMaps_.hashNestedP2WSH_)
   {
      BinaryWriter bw;
      bw.put_uint8_t(prefix);
      bw.put_BinaryDataRef(hashIndexPair.first);
      addrHashSet.insert(bw.getData());
   }

   for (auto& hashIndexPair : hashMaps_.hashP2WSH_)
   {
      addrHashSet.insert(hashIndexPair.first);
   }

   return addrHashSet;
}

////////////////////////////////////////////////////////////////////////////////
BinaryData AssetWallet_Multisig::getPrefixedHashForIndex(
   unsigned index) const
{
   auto assetPtr = getAssetForIndex(index);
   auto assetMS = dynamic_pointer_cast<AssetEntry_Multisig>(assetPtr);
   if (assetMS == nullptr)
      throw WalletException("unexpected asset type");

   BinaryWriter bw;
   bw.put_uint8_t(BlockDataManagerConfig::getScriptHashPrefix());

   switch (default_aet_)
   {
   case AddressEntryType_Nested_Multisig:
      bw.put_BinaryData(assetMS->getHash160());
      break;

   case AddressEntryType_P2WSH:
     return assetMS->getHash256();

   case AddressEntryType_Nested_P2WSH:
      bw.put_BinaryData(assetMS->getP2WSHScriptH160());
      break;

   default:
      throw WalletException("invalid aet");
   }

   return bw.getData();
}

////////////////////////////////////////////////////////////////////////////////
shared_ptr<AssetEntry> AssetWallet::getAssetForIndex(unsigned index) const
{
   ReentrantLock lock(this);

   auto iter = assets_.find(index);
   if (iter == assets_.end())
      throw WalletException("invalid asset index");

   return iter->second;
}

////////////////////////////////////////////////////////////////////////////////
const BinaryData& AssetWallet::getP2SHScriptForHash(const BinaryData& script)
{
   fillHashIndexMap();

   auto getEntryPtr = [this](const BinaryData& hash)->shared_ptr<AssetEntry>
   {
      auto iter1 = hashMaps_.hashNestedP2PK_.find(hash);
      if (iter1 != hashMaps_.hashNestedP2PK_.end())
         return getAssetForIndex(iter1->second);

      auto iter2 = hashMaps_.hashNestedP2WPKH_.find(hash);
      if (iter2 != hashMaps_.hashNestedP2WPKH_.end())
         return getAssetForIndex(iter2->second);

      auto iter3 = hashMaps_.hashNestedMultisig_.find(hash);
      if (iter3 != hashMaps_.hashNestedMultisig_.end())
         return getAssetForIndex(iter3->second);

      auto iter4 = hashMaps_.hashNestedP2WSH_.find(hash);
      if (iter4 != hashMaps_.hashNestedP2WSH_.end())
         return getAssetForIndex(iter4->second);

      return nullptr;
   };

   auto&& hash = BtcUtils::getTxOutRecipientAddr(script);
   auto entryPtr = getEntryPtr(hash);

   if (entryPtr == nullptr)
      throw WalletException("unkonwn hash");

   auto assetSingle = dynamic_pointer_cast<AssetEntry_Single>(entryPtr);
   if (assetSingle != nullptr)
   {
      auto& p2pkHash = assetSingle->getP2PKScriptH160();
      if (p2pkHash == hash)
         return assetSingle->getP2PKScript();
      else
         return assetSingle->getWitnessScript();
   }

   auto assetMS = dynamic_pointer_cast<AssetEntry_Multisig>(entryPtr);
   if (assetMS == nullptr)
      throw WalletException("unexpected entry type");

   auto& nestedP2WSHhash = assetMS->getP2WSHScriptH160();
   if (nestedP2WSHhash == hash)
      return assetMS->getP2WSHScript();
   else
      return assetMS->getScript();
}

////////////////////////////////////////////////////////////////////////////////
const BinaryData& AssetWallet::getNestedSWAddrForIndex(
   unsigned chainIndex)
{
   ReentrantLock lock(this);

   auto assetPtr = getAssetForIndex(chainIndex);
   auto addrEntry = getAddressEntryForAsset(
      assetPtr, AddressEntryType_Nested_P2WPKH);

   return addrEntry->getAddress();
}

////////////////////////////////////////////////////////////////////////////////
const BinaryData& AssetWallet::getNestedP2PKAddrForIndex(
   unsigned chainIndex)
{
   ReentrantLock lock(this);

   auto assetPtr = getAssetForIndex(chainIndex);
   auto addrEntry = getAddressEntryForAsset(
      assetPtr, AddressEntryType_Nested_P2PK);

   return addrEntry->getAddress();
}

////////////////////////////////////////////////////////////////////////////////
const BinaryData& AssetWallet::getP2PKHAddrForIndex(unsigned chainIndex)
{
   ReentrantLock lock(this);

   auto assetPtr = getAssetForIndex(chainIndex);
   auto addrEntry = getAddressEntryForAsset(
      assetPtr, AddressEntryType_P2PKH);

   return addrEntry->getAddress();
}

////////////////////////////////////////////////////////////////////////////////
int AssetWallet::getLastComputedIndex(void) const
{
   if (getAssetCount() == 0)
      return -1;

   auto iter = assets_.rbegin();
   return iter->first;
}

////////////////////////////////////////////////////////////////////////////////
string AssetWallet::getID(void) const
{
   return string(walletID_.getCharPtr(), walletID_.getSize());
}

////////////////////////////////////////////////////////////////////////////////
void AssetWallet::extendPublicChain(unsigned count)
{
   ReentrantLock lock(this);

   //add *count* entries to address chain
   if (assets_.size() == 0)
      throw WalletException("empty asset map");
   
   if (count == 0)
      return;

   extendPublicChain(assets_.rbegin()->second, count);
}

////////////////////////////////////////////////////////////////////////////////
bool AssetWallet::extendPublicChainToIndex(unsigned count)
{
   ReentrantLock lock(this);

   //make address chain at least *count* long
   auto lastComputedIndex = max(getLastComputedIndex(), 0);
   if (lastComputedIndex > count)
      return false;

   auto toCompute = count - lastComputedIndex;

   extendPublicChain(toCompute);
   return true;
}

////////////////////////////////////////////////////////////////////////////////
void AssetWallet::extendPublicChain(
   shared_ptr<AssetEntry> assetPtr, unsigned count)
{
   if (count == 0)
      return;

   ReentrantLock lock(this);

   auto&& assetVec = derScheme_->extendPublicChain(assetPtr, count);

   LMDBEnv::Transaction tx(dbEnv_.get(), LMDB::ReadWrite);

   {
      for (auto& asset : assetVec)
      {
         auto id = asset->getId();
         auto iter = assets_.find(id);
         if (iter != assets_.end())
            continue;

         assets_.insert(make_pair(
            id, asset));
      }
   }

   updateOnDiskAssets();
}

////////////////////////////////////////////////////////////////////////////////
void AssetWallet::extendPrivateChain(unsigned count)
{
   ReentrantLock lock(this);
   auto topAsset = getLastAssetWithPrivateKey();

   extendPrivateChain(topAsset, count);
}

////////////////////////////////////////////////////////////////////////////////
void AssetWallet::extendPrivateChainToIndex(unsigned id)
{
   ReentrantLock lock(this);

   auto topAsset = getLastAssetWithPrivateKey();
   auto count = id - topAsset->getId();

   extendPrivateChain(topAsset, count);
}

////////////////////////////////////////////////////////////////////////////////
void AssetWallet::extendPrivateChain(shared_ptr<AssetEntry> asset, unsigned count)
{
   if (count == 0)
      return;

   ReentrantLock lock(this);
   auto&& assetVec = derScheme_->extendPrivateChain(asset, count);

   LMDBEnv::Transaction tx(dbEnv_.get(), LMDB::ReadWrite);

   {
      for (auto& asset : assetVec)
      {
         auto id = asset->getId();
         auto iter = assets_.find(id);
         if (iter != assets_.end())
         {
            //do not overwrite an existing asset that already has a privkey
            if (iter->second->hasPrivateKey())
            {
               continue;
            }
            else
            {
               iter->second = asset;
               continue;
            }
         }

         assets_.insert(make_pair(
            id, asset));
      }
   }

   updateOnDiskAssets();
}

////////////////////////////////////////////////////////////////////////////////
shared_ptr<AssetEntry> AssetWallet::getLastAssetWithPrivateKey() const
{
   ReentrantLock lock(this);

   auto assetIter = assets_.rbegin();
   while (assetIter != assets_.rend())
   {
      if (assetIter->second->hasPrivateKey())
         return assetIter->second;

      ++assetIter;
   }

   throw runtime_error("no asset with private keys");
   return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
ReentrantLock AssetWallet::lockDecryptedContainer(void)
{
   return move(ReentrantLock(decryptedData_.get()));
}

////////////////////////////////////////////////////////////////////////////////
ReentrantLock AssetWallet_Multisig::lockDecryptedContainer(void)
{
   auto lock = AssetWallet::lockDecryptedContainer();

   auto derSchemeMS = 
      dynamic_pointer_cast<DerivationScheme_Multisig>(derScheme_);
   if (derSchemeMS == nullptr)
      throw runtime_error("unexpected der scheme type");

   auto&& walletIDs = derSchemeMS->getWalletIDs();
   for (auto& id : walletIDs)
   {
      auto wallet = derSchemeMS->getSubWalletPtr(id);
      decryptedData_->lockOther(wallet->decryptedData_);
   }

   return lock;
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
bool AssetWallet_Single::setImport(
   int importID, const SecureBinaryData& pubkey)
{
   auto importIndex = convertToImportIndex(importID);
   
   ReentrantLock lock(this);

   auto assetIter = assets_.find(importIndex);
   if (assetIter != assets_.end())
      return false;

   auto pubkey_copy = pubkey;
   SecureBinaryData empty_privkey;
   auto newAsset = make_shared<AssetEntry_Single>(
      importIndex, pubkey_copy, empty_privkey, nullptr);

   assets_.insert(make_pair(importIndex, newAsset));
   writeAssetEntry(newAsset);

   return true;
}

////////////////////////////////////////////////////////////////////////////////
bool AssetWallet_Multisig::setImport(
   int importID, const SecureBinaryData& pubkey)
{
   throw WalletException("setImport not implemented for multisig wallets");
   return false;
}

////////////////////////////////////////////////////////////////////////////////
int AssetWallet::convertToImportIndex(int importID)
{
   return INT32_MIN + importID;
}

////////////////////////////////////////////////////////////////////////////////
int AssetWallet::convertFromImportIndex(int importID)
{
   return INT32_MAX + 1 + importID;
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
void AssetWallet::changeMasterPassphrase(const SecureBinaryData& newPassphrase)
{
   /***changes encryption of the wallet's master key***/
   if (assets_.size() == 0)
      throw runtime_error("no assets in wallet");
   
   auto lock = lockDecryptedContainer();
   auto&& masterKeyId = assets_[0]->getPrivateEncryptionKeyId();
   decryptedData_->encryptEncryptionKey(masterKeyId, newPassphrase);
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
//// DerivationScheme
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
DerivationScheme::~DerivationScheme() 
{}

////////////////////////////////////////////////////////////////////////////////
shared_ptr<DerivationScheme> DerivationScheme::deserialize(BinaryDataRef data)
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
         chainCode, nullptr);

      break;
   }

   case DERIVATIONSCHEME_MULTISIG:
   {
      //grab n, m
      auto m = brr.get_uint32_t();
      auto n = brr.get_uint32_t();

      set<BinaryData> ids;
      while (brr.getSizeRemaining() > 0)
      {
         auto len = brr.get_var_int();
         auto&& id = brr.get_BinaryData(len);
         ids.insert(move(id));
      }

      if (ids.size() != n)
         throw DerivationSchemeDeserException("id count mismatch");

      derScheme = make_shared<DerivationScheme_Multisig>(ids, n, m);

      break;
   }

   default:
      throw DerivationSchemeDeserException("unsupported derivation scheme");
   }

   return derScheme;
}

////////////////////////////////////////////////////////////////////////////////
vector<shared_ptr<AssetEntry>> DerivationScheme_ArmoryLegacy::extendPublicChain(
   shared_ptr<AssetEntry> firstAsset, unsigned count)
{
   auto nextAsset = [this](
      shared_ptr<AssetEntry> assetPtr)->shared_ptr<AssetEntry>
   {
      auto assetSingle =
         dynamic_pointer_cast<AssetEntry_Single>(assetPtr);

      //get pubkey
      auto pubkey = assetSingle->getPubKey();
      auto& pubkeyData = pubkey->getUncompressedKey();

      auto&& nextPubkey = CryptoECDSA().ComputeChainedPublicKey(
         pubkeyData, chainCode_, nullptr);

      return make_shared<AssetEntry_Single>(
         assetSingle->getId() + 1,
         nextPubkey, nullptr);
   };
   
   vector<shared_ptr<AssetEntry>> assetVec;
   auto currentAsset = firstAsset;

   for (unsigned i = 0; i < count; i++)
   { 
      currentAsset = nextAsset(currentAsset);
      assetVec.push_back(currentAsset);
   }

   return assetVec;
}

////////////////////////////////////////////////////////////////////////////////
vector<shared_ptr<AssetEntry>> 
DerivationScheme_ArmoryLegacy::extendPrivateChain(
shared_ptr<AssetEntry> firstAsset, unsigned count)
{
   //throws is the wallet is locked or the asset is missing its private key

   auto nextAsset = [this](
      shared_ptr<AssetEntry> assetPtr)->shared_ptr<AssetEntry>
   {
      //sanity checks
      auto assetSingle =
         dynamic_pointer_cast<AssetEntry_Single>(assetPtr);

      auto privkey = assetSingle->getPrivKey();
      if (privkey == nullptr)
         throw AssetUnavailableException();
      auto& privkeyData = 
         decryptedDataContainer_->getDecryptedPrivateKey(privkey);

      //chain the private key
      auto&& nextPrivkeySBD = move(CryptoECDSA().ComputeChainedPrivateKey(
         privkeyData, chainCode_));
      
      //compute its pubkey
      auto&& nextPubkey = CryptoECDSA().ComputePublicKey(nextPrivkeySBD);

      //encrypt the new privkey
      auto&& newCypher = privkey->copyCypher();
      auto&& encryptedNextPrivKey = decryptedDataContainer_->encryptData(
         newCypher.get(), nextPrivkeySBD);

      //clear the unencrypted privkey object
      nextPrivkeySBD.clear();

      //instantiate new encrypted key object
      auto id_int = assetSingle->getId() + 1;
      auto nextPrivKey = make_shared<Asset_PrivateKey>(id_int,
         encryptedNextPrivKey, move(newCypher));

      //instantiate and return new asset entry
      return make_shared<AssetEntry_Single>(
         assetSingle->getId() + 1,
         nextPubkey, nextPrivKey);
   };
   
   if (decryptedDataContainer_ == nullptr)
      throw AssetUnavailableException();

   ReentrantLock lock(decryptedDataContainer_.get());

   vector<shared_ptr<AssetEntry>> assetVec;
   auto currentAsset = firstAsset;

   for (unsigned i = 0; i < count; i++)
   {
      currentAsset = nextAsset(currentAsset);
      assetVec.push_back(currentAsset);
   }

   return assetVec;
}

////////////////////////////////////////////////////////////////////////////////
vector<shared_ptr<AssetEntry>> DerivationScheme_Multisig::extendPublicChain(
   shared_ptr<AssetEntry> firstAsset, unsigned count)
{
   //synchronize wallet chains length
   unsigned bottom = UINT32_MAX;
   auto total = firstAsset->getId() + 1 + count; 
 
   for (auto& wltPtr : wallets_)
   {
      wltPtr.second->extendPublicChain(
         total - wltPtr.second->getAssetCount());
   }

   vector<shared_ptr<AssetEntry>> assetVec;
   for (unsigned i = firstAsset->getId() + 1; i < total; i++)
      assetVec.push_back(getAssetForIndex(i));

   return assetVec;
}

////////////////////////////////////////////////////////////////////////////////
vector<shared_ptr<AssetEntry>> DerivationScheme_Multisig::extendPrivateChain(
   shared_ptr<AssetEntry> firstAsset, unsigned count)
{
   //synchronize wallet chains length
   unsigned bottom = UINT32_MAX;
   auto total = firstAsset->getId() + 1 + count;

   for (auto& wltPtr : wallets_)
      wltPtr.second->extendPrivateChain(count);

   vector<shared_ptr<AssetEntry>> assetVec;
   for (unsigned i = firstAsset->getId() + 1; i < total; i++)
   {
      auto assetptr = getAssetForIndex(i);
      assetptr->doNotCommit(); //ms assets aren't commited to the disk as is
      assetVec.push_back(getAssetForIndex(i));
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
BinaryData DerivationScheme_Multisig::serialize() const
{
   if (walletIDs_.size() != n_)
      throw WalletException("multisig wallet is missing subwallets");

   BinaryWriter bw;
   bw.put_uint8_t(DERIVATIONSCHEME_MULTISIG);
   bw.put_uint32_t(m_);
   bw.put_uint32_t(n_);
   
   for (auto& id : walletIDs_)
   {
      bw.put_var_int(id.getSize());
      bw.put_BinaryData(id);
   }

   BinaryWriter bwFinal;
   bwFinal.put_var_int(bw.getSize());
   bwFinal.put_BinaryData(bw.getData());

   return bwFinal.getData();
}

////////////////////////////////////////////////////////////////////////////////
void DerivationScheme_Multisig::setSubwalletPointers(
   map<BinaryData, shared_ptr<AssetWallet_Single>> ptrMap)
{
   set<BinaryData> ids;
   for (auto& wltPtr : ptrMap)
      ids.insert(wltPtr.first);

   if (ids != walletIDs_)
      throw DerivationSchemeDeserException("ids set mismatch");

   wallets_ = ptrMap;
}

////////////////////////////////////////////////////////////////////////////////
shared_ptr<AssetWallet_Single> DerivationScheme_Multisig::getSubWalletPtr(
   const BinaryData& id) const
{
   auto iter = wallets_.find(id);
   if (iter == wallets_.end())
      throw DerivationSchemeDeserException("unknown id");

   return iter->second;
}

////////////////////////////////////////////////////////////////////////////////
shared_ptr<AssetEntry_Multisig> DerivationScheme_Multisig::getAssetForIndex(
   unsigned index) const
{
   //gather assets
   map<BinaryData, shared_ptr<AssetEntry>> assetMap;

   for (auto wltPtr : wallets_)
   {
      auto asset = wltPtr.second->getAssetForIndex(index);
      assetMap.insert(make_pair(wltPtr.first, asset));
   }

   //create asset
   return make_shared<AssetEntry_Multisig>(index, assetMap, m_, n_);
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
//// AddressEntry
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
AddressEntry::~AddressEntry()
{}

////////////////////////////////////////////////////////////////////////////////
const BinaryData& AddressEntry_P2PKH::getPrefixedHash() const
{
   if (hash_.getSize() == 0)
   {
      auto assetSingle = dynamic_pointer_cast<AssetEntry_Single>(asset_);
      if (assetSingle == nullptr)
         throw WalletException("unexpected asset entry type");

      auto& h160 = assetSingle->getHash160Uncompressed();

      //get and prepend network byte
      auto networkByte = BlockDataManagerConfig::getPubkeyHashPrefix();

      hash_.append(networkByte);
      hash_.append(h160);
   }

   return hash_;
}

////////////////////////////////////////////////////////////////////////////////
const BinaryData& AddressEntry_P2PKH::getAddress() const
{
   if (address_.getSize() == 0)
      address_ = move(BtcUtils::scrAddrToBase58(getPrefixedHash()));

   return address_;
}

////////////////////////////////////////////////////////////////////////////////
shared_ptr<ScriptRecipient> AddressEntry_P2PKH::getRecipient(
   uint64_t value) const
{
   auto assetSingle = dynamic_pointer_cast<AssetEntry_Single>(asset_);
   if (assetSingle == nullptr)
      throw WalletException("unexpected asset entry type");

   auto& h160 = assetSingle->getHash160Uncompressed();
   return make_shared<Recipient_P2PKH>(h160, value);
}

////////////////////////////////////////////////////////////////////////////////
const BinaryData& AddressEntry_P2WPKH::getPrefixedHash() const
{
   if (hash_.getSize() == 0)
   {
      auto assetSingle = dynamic_pointer_cast<AssetEntry_Single>(asset_);
      if (assetSingle == nullptr)
         throw WalletException("unexpected asset entry type");

      //no address standard for SW yet, consider BIP142
      hash_ = assetSingle->getHash160Compressed();
   }

   return hash_;
}

////////////////////////////////////////////////////////////////////////////////
const BinaryData& AddressEntry_P2WPKH::getAddress() const
{
   if (address_.getSize() == 0)
      address_ = getPrefixedHash();

   return address_;
}

////////////////////////////////////////////////////////////////////////////////
shared_ptr<ScriptRecipient> AddressEntry_P2WPKH::getRecipient(
   uint64_t value) const
{
   auto assetSingle = dynamic_pointer_cast<AssetEntry_Single>(asset_);
   if (assetSingle == nullptr)
      throw WalletException("unexpected asset entry type");

   auto& h160 = assetSingle->getHash160Compressed();
   return make_shared<Recipient_P2WPKH>(h160, value);
}

////////////////////////////////////////////////////////////////////////////////
const BinaryData& AddressEntry_Nested_Multisig::getPrefixedHash() const
{
   auto prefix = BlockDataManagerConfig::getScriptHashPrefix();

   if (hash_.getSize() == 0)
   {
      switch (asset_->getType())
      {
      case AssetEntryType_Single:
      {
         auto assetSingle = dynamic_pointer_cast<AssetEntry_Single>(asset_);
         if (assetSingle == nullptr)
            throw WalletException("unexpected asset entry type");

         hash_.append(prefix);
         hash_.append(assetSingle->getHash160Compressed());
         break;
      }

      case AssetEntryType_Multisig:
      {
         auto assetMS = dynamic_pointer_cast<AssetEntry_Multisig>(asset_);
         if (assetMS == nullptr)
            throw WalletException("unexpected asset entry type");

         hash_.append(prefix);
         hash_.append(assetMS->getHash160());
         break;
      }

      default:
         throw WalletException("unexpected asset type");
      }
   }

   return hash_;
}

////////////////////////////////////////////////////////////////////////////////
const BinaryData& AddressEntry_Nested_Multisig::getAddress() const
{
   auto prefix = BlockDataManagerConfig::getScriptHashPrefix();

   if (address_.getSize() == 0)
      address_ = move(BtcUtils::scrAddrToBase58(getPrefixedHash()));

   return address_;
}

////////////////////////////////////////////////////////////////////////////////
shared_ptr<ScriptRecipient> AddressEntry_Nested_Multisig::getRecipient(
   uint64_t value) const
{
   BinaryDataRef h160;
   switch (asset_->getType())
   {
   case AssetEntryType_Multisig:
   {
      auto assetMS = dynamic_pointer_cast<AssetEntry_Multisig>(asset_);
      if (assetMS == nullptr)
         throw WalletException("unexpected asset entry type");

      h160 = assetMS->getHash160();
      break;
   }

   default:
      throw WalletException("unexpected asset type");
   }

   return make_shared<Recipient_P2SH>(h160, value);
}

////////////////////////////////////////////////////////////////////////////////
size_t AddressEntry_Nested_Multisig::getInputSize() const
{
   switch (asset_->getType())
   {
   case AssetEntryType_Multisig:
   {
      auto assetMS = dynamic_pointer_cast<AssetEntry_Multisig>(asset_);
      if (assetMS == nullptr)
         throw WalletException("unexpected asset entry type");

      auto m = assetMS->getM();

      size_t size = assetMS->getScript().getSize() + 2;
      size += 73 * m + 40; //m sigs + outpoint

      return size;
   }

   default:
      throw WalletException("unexpected asset type");
   }

   return SIZE_MAX;
}

////////////////////////////////////////////////////////////////////////////////
const BinaryData& AddressEntry_P2WSH::getPrefixedHash() const
{
   if (hash_.getSize() == 0)
   {
      switch (asset_->getType())
      {
      case AssetEntryType_Multisig:
      {
         auto assetMS = dynamic_pointer_cast<AssetEntry_Multisig>(asset_);
         if (assetMS == nullptr)
            throw WalletException("unexpected asset entry type");

         hash_ = move(assetMS->getHash256());
         break;
      }

      default:
         throw WalletException("unexpected asset type");
      }
   }

   return hash_;
}

////////////////////////////////////////////////////////////////////////////////
const BinaryData& AddressEntry_P2WSH::getAddress() const
{
   if (address_.getSize() == 0)
      address_ = getPrefixedHash();

   return address_;
}

////////////////////////////////////////////////////////////////////////////////
shared_ptr<ScriptRecipient> AddressEntry_P2WSH::getRecipient(
   uint64_t value) const
{
   BinaryDataRef scriptHash;
   switch (asset_->getType())
   {
   case AssetEntryType_Multisig:
   {
      auto assetMS = dynamic_pointer_cast<AssetEntry_Multisig>(asset_);
      if (assetMS == nullptr)
         throw WalletException("unexpected asset entry type");

      scriptHash = assetMS->getHash256();
      break;
   }

   default:
      throw WalletException("unexpected asset type");
   }

   return make_shared<Recipient_PW2SH>(scriptHash, value);
}

////////////////////////////////////////////////////////////////////////////////
size_t AddressEntry_P2WSH::getWitnessDataSize() const
{
   switch (asset_->getType())
   {
   case AssetEntryType_Multisig:
   {
      auto assetMS = dynamic_pointer_cast<AssetEntry_Multisig>(asset_);
      if (assetMS == nullptr)
         throw WalletException("unexpected asset entry type");

      auto m = assetMS->getM();

      size_t size = assetMS->getScript().getSize() + 2;
      size += 73 * m + 2;
      
      return size;
   }

   default:
      throw WalletException("unexpected asset type");
   }

   return SIZE_MAX;
}

////////////////////////////////////////////////////////////////////////////////
const BinaryData& AddressEntry_Nested_P2WPKH::getPrefixedHash() const
{
   uint8_t prefix = BlockDataManagerConfig::getScriptHashPrefix();

   if (hash_.getSize() == 0)
   {
      switch (asset_->getType())
      {
      case AssetEntryType_Single:
      {
         auto assetSingle = dynamic_pointer_cast<AssetEntry_Single>(asset_);
         if (assetSingle == nullptr)
            throw WalletException("unexpected asset entry type");

         hash_.append(prefix);
         hash_.append(assetSingle->getWitnessScriptH160());

         break;
      }

      default:
         throw WalletException("unexpected asset type");
      }
   }

   return hash_;
}

////////////////////////////////////////////////////////////////////////////////
const BinaryData& AddressEntry_Nested_P2WPKH::getAddress() const
{
   if (address_.getSize() == 0)
      address_ = move(BtcUtils::scrAddrToBase58(getPrefixedHash()));

   return address_;
}

////////////////////////////////////////////////////////////////////////////////
shared_ptr<ScriptRecipient> AddressEntry_Nested_P2WPKH::getRecipient(
   uint64_t value) const
{
   BinaryDataRef scriptHash;

   switch (asset_->getType())
   {
   case AssetEntryType_Single:
   {
      auto assetSingle = dynamic_pointer_cast<AssetEntry_Single>(asset_);
      if (assetSingle == nullptr)
         throw WalletException("unexpected asset entry type");

      scriptHash = assetSingle->getWitnessScriptH160();
      
      break;
   }

   default:
      throw WalletException("unexpected asset type");
   }

   return make_shared<Recipient_P2SH>(scriptHash, value);
}

////////////////////////////////////////////////////////////////////////////////
const BinaryData& AddressEntry_Nested_P2WSH::getPrefixedHash() const
{
   uint8_t prefix = BlockDataManagerConfig::getScriptHashPrefix();

   if (hash_.getSize() == 0)
   {
      switch (asset_->getType())
      {
      case AssetEntryType_Multisig:
      {
         auto assetMS = dynamic_pointer_cast<AssetEntry_Multisig>(asset_);
         if (assetMS == nullptr)
            throw WalletException("unexpected asset entry type");

         hash_.append(prefix);
         hash_.append(assetMS->getP2WSHScriptH160());

         break;
      }

      default:
         throw WalletException("unexpected asset type");
      }
   }

   return hash_;
}

////////////////////////////////////////////////////////////////////////////////
const BinaryData& AddressEntry_Nested_P2WSH::getAddress() const
{
   if (address_.getSize() == 0)
      address_ = move(BtcUtils::scrAddrToBase58(getPrefixedHash()));

   return address_;
}

////////////////////////////////////////////////////////////////////////////////
shared_ptr<ScriptRecipient> AddressEntry_Nested_P2WSH::getRecipient(
   uint64_t value) const
{
   BinaryDataRef scriptHash;

   switch (asset_->getType())
   {
   case AssetEntryType_Multisig:
   {
      auto assetMS = dynamic_pointer_cast<AssetEntry_Multisig>(asset_);
      if (assetMS == nullptr)
         throw WalletException("unexpected asset entry type");

      scriptHash = assetMS->getP2WSHScriptH160();

      break;
   }

   default:
      throw WalletException("unexpected asset type");
   }

   return make_shared<Recipient_P2SH>(scriptHash, value);
}

////////////////////////////////////////////////////////////////////////////////
size_t AddressEntry_Nested_P2WSH::getWitnessDataSize() const
{
   switch (asset_->getType())
   {
   case AssetEntryType_Multisig:
   {
      auto assetMS = dynamic_pointer_cast<AssetEntry_Multisig>(asset_);
      if (assetMS == nullptr)
         throw WalletException("unexpected asset entry type");

      auto m = assetMS->getM();

      size_t size = assetMS->getScript().getSize() + 2;
      size += 73 * m + 2;

      return size;
   }

   default:
      throw WalletException("unexpected asset type");
   }

   return SIZE_MAX;
}

////////////////////////////////////////////////////////////////////////////////
const BinaryData& AddressEntry_Nested_P2PK::getPrefixedHash() const
{
   uint8_t prefix = BlockDataManagerConfig::getScriptHashPrefix();

   if (hash_.getSize() == 0)
   {
      switch (asset_->getType())
      {
      case AssetEntryType_Single:
      {
         auto assetSingle = dynamic_pointer_cast<AssetEntry_Single>(asset_);
         if (assetSingle == nullptr)
            throw WalletException("unexpected asset entry type");

         hash_.append(prefix);
         hash_.append(assetSingle->getP2PKScriptH160());

         break;
      }

      default:
         throw WalletException("unexpected asset type");
      }
   }

   return hash_;
}

////////////////////////////////////////////////////////////////////////////////
const BinaryData& AddressEntry_Nested_P2PK::getAddress() const
{
   if (address_.getSize() == 0)
      address_ = move(BtcUtils::scrAddrToBase58(getPrefixedHash()));

   return address_;
}

////////////////////////////////////////////////////////////////////////////////
shared_ptr<ScriptRecipient> AddressEntry_Nested_P2PK::getRecipient(
   uint64_t value) const
{
   BinaryDataRef scriptHash;

   switch (asset_->getType())
   {
   case AssetEntryType_Single:
   {
      auto assetSingle = dynamic_pointer_cast<AssetEntry_Single>(asset_);
      if (assetSingle == nullptr)
         throw WalletException("unexpected asset entry type");

      scriptHash = assetSingle->getP2PKScriptH160();

      break;
   }

   default:
      throw WalletException("unexpected asset type");
   }

   return make_shared<Recipient_P2SH>(scriptHash, value);
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
//// Asset
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
Asset::~Asset()
{}

Asset_EncryptedData::~Asset_EncryptedData()
{}

////////////////////////////////////////////////////////////////////////////////
unique_ptr<DecryptedEncryptionKey> Asset_EncryptionKey::decrypt(
   const SecureBinaryData& key) const
{
   auto decryptedData = cypher_->decrypt(key, data_);
   auto decrPtr = make_unique<DecryptedEncryptionKey>(decryptedData);
   return move(decrPtr);
}

////////////////////////////////////////////////////////////////////////////////
unique_ptr<DecryptedPrivateKey> Asset_PrivateKey::decrypt(
   const SecureBinaryData& key) const
{
   auto&& decryptedData = cypher_->decrypt(key, data_);
   auto decrPtr = make_unique<DecryptedPrivateKey>(id_, decryptedData);
   return move(decrPtr);
}

////////////////////////////////////////////////////////////////////////////////
BinaryData Asset_PublicKey::serialize() const
{
   BinaryWriter bw;
  
   bw.put_var_int(uncompressed_.getSize() + 1);
   bw.put_uint8_t(PUBKEY_UNCOMPRESSED_BYTE);
   bw.put_BinaryData(uncompressed_);

   bw.put_var_int(compressed_.getSize() + 1);
   bw.put_uint8_t(PUBKEY_COMPRESSED_BYTE);
   bw.put_BinaryData(compressed_);

   return bw.getData();
}

////////////////////////////////////////////////////////////////////////////////
BinaryData Asset_PrivateKey::serialize() const
{
   BinaryWriter bw;
   bw.put_uint8_t(PRIVKEY_BYTE);
   bw.put_int32_t(id_);
   bw.put_var_int(data_.getSize());
   bw.put_BinaryData(data_);

   auto&& cypherData = cypher_->serialize();
   bw.put_var_int(cypherData.getSize());
   bw.put_BinaryData(cypherData);

   BinaryWriter finalBw;
   finalBw.put_var_int(bw.getSize());
   finalBw.put_BinaryDataRef(bw.getDataRef());
   return finalBw.getData();
}

////////////////////////////////////////////////////////////////////////////////
BinaryData Asset_EncryptionKey::serialize() const
{
   BinaryWriter bw;
   bw.put_uint8_t(ENCRYPTIONKEY_BYTE);
   bw.put_var_int(id_.getSize());
   bw.put_BinaryData(id_);
   bw.put_var_int(data_.getSize());
   bw.put_BinaryData(data_);

   auto&& cypherData = cypher_->serialize();
   bw.put_var_int(cypherData.getSize());
   bw.put_BinaryData(cypherData);

   BinaryWriter finalBw;
   finalBw.put_var_int(bw.getSize());
   finalBw.put_BinaryDataRef(bw.getDataRef());
   return finalBw.getData();
}

////////////////////////////////////////////////////////////////////////////////
bool Asset_PrivateKey::isSame(Asset_EncryptedData* const asset) const
{
   auto asset_ed = dynamic_cast<Asset_PrivateKey*>(asset);
   if (asset_ed == nullptr)
      return false;

   return id_ == asset_ed->id_ && data_ == asset_ed->data_ &&
      cypher_->isSame(asset_ed->cypher_.get());
}

////////////////////////////////////////////////////////////////////////////////
bool Asset_EncryptionKey::isSame(Asset_EncryptedData* const asset) const
{
   auto asset_ed = dynamic_cast<Asset_EncryptionKey*>(asset);
   if (asset_ed == nullptr)
      return false;

   return id_ == asset_ed->id_ && data_ == asset_ed->data_ &&
      cypher_->isSame(asset_ed->cypher_.get());
}

////////////////////////////////////////////////////////////////////////////////
shared_ptr<Asset_EncryptedData> Asset_EncryptedData::deserialize(
   const BinaryDataRef& data)
{
   BinaryRefReader brr(data);

   //grab size
   auto totalLen = brr.get_var_int();
   return deserialize(totalLen, brr.get_BinaryDataRef(brr.getSizeRemaining()));
}

////////////////////////////////////////////////////////////////////////////////
shared_ptr<Asset_EncryptedData> Asset_EncryptedData::deserialize(
   size_t totalLen, const BinaryDataRef& data)
{
   BinaryRefReader brr(data);

   //check size
   if (totalLen != brr.getSizeRemaining())
      throw runtime_error("invalid serialized encrypted data len");

   //return ptr
   shared_ptr<Asset_EncryptedData> assetPtr = nullptr;

   //prefix
   auto prefix = brr.get_uint8_t();

   switch (prefix)
   {
   case PRIVKEY_BYTE:
   {
      //id
      auto&& id = brr.get_int32_t();

      //data
      auto len = brr.get_var_int();
      auto&& data = brr.get_SecureBinaryData(len);

      //cypher
      len = brr.get_var_int();
      if (len > brr.getSizeRemaining())
         throw runtime_error("invalid serialized encrypted data len");
      auto&& cypher = Cypher::deserialize(brr);

      //ptr
      assetPtr = make_shared<Asset_PrivateKey>(id, data, move(cypher));

      break;
   }

   case ENCRYPTIONKEY_BYTE:
   {
      //id
      auto len = brr.get_var_int();
      auto&& id = brr.get_BinaryData(len);

      //data
      len = brr.get_var_int();
      auto&& data = brr.get_SecureBinaryData(len);

      //cypher
      len = brr.get_var_int();
      if (len > brr.getSizeRemaining())
         throw runtime_error("invalid serialized encrypted data len");
      auto&& cypher = Cypher::deserialize(brr);

      //ptr
      assetPtr = make_shared<Asset_EncryptionKey>(id, data, move(cypher));

      break;
   }

   default:
      throw runtime_error("unexpected encrypted data prefix");
   }

   return assetPtr;
}


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
//// AssetEntry
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
AssetEntry::~AssetEntry(void)
{}

////////////////////////////////////////////////////////////////////////////////
const BinaryData& AssetEntry_Single::getHash160Uncompressed() const
{
   if (h160Uncompressed_.getSize() == 0)
      h160Uncompressed_ = 
         move(BtcUtils::getHash160(pubkey_->getUncompressedKey()));

   return h160Uncompressed_;
}

////////////////////////////////////////////////////////////////////////////////
const BinaryData& AssetEntry_Single::getHash160Compressed() const
{
   if (h160Compressed_.getSize() == 0)
      h160Compressed_ =
         move(BtcUtils::getHash160(pubkey_->getCompressedKey()));

   return h160Compressed_;
}

////////////////////////////////////////////////////////////////////////////////
const BinaryData& AssetEntry_Single::getWitnessScript() const
{
   if (witnessScript_.getSize() == 0)
   {
      auto& hash = getHash160Compressed();
      Recipient_P2WPKH recipient(hash, 0);

      auto& script = recipient.getSerializedScript();

      witnessScript_ = move(script.getSliceCopy(9, script.getSize() - 9));
   }

   return witnessScript_;
}

////////////////////////////////////////////////////////////////////////////////
const BinaryData& AssetEntry_Single::getWitnessScriptH160() const
{
   if (witnessScriptH160_.getSize() == 0)
      witnessScriptH160_ =
         move(BtcUtils::getHash160(getWitnessScript()));

   return witnessScriptH160_;
}

////////////////////////////////////////////////////////////////////////////////
const BinaryData& AssetEntry_Single::getP2PKScript() const
{
   if (p2pkScript_.getSize() == 0)
   {
      p2pkScript_.append(33); //push data opcode for pubkey len
      p2pkScript_.append(pubkey_->getCompressedKey()); //compressed pubkey
      p2pkScript_.append(OP_CHECKSIG); 
   }

   return p2pkScript_;
}

////////////////////////////////////////////////////////////////////////////////
const BinaryData& AssetEntry_Single::getP2PKScriptH160() const
{
   if (p2pkScriptH160_.getSize() == 0)
      p2pkScriptH160_ =
         move(BtcUtils::getHash160(getP2PKScript()));

   return p2pkScriptH160_;
}

////////////////////////////////////////////////////////////////////////////////
AddressEntryType AssetEntry_Single::getAddressTypeForHash(
   BinaryDataRef hashRef) const
{
      auto& h160Unc = getHash160Uncompressed();
   if (hashRef == h160Unc)
      return AddressEntryType_P2PKH;
   
   auto& nestedP2PKScriptHash = getP2PKScriptH160();
   if (hashRef == nestedP2PKScriptHash)
      return AddressEntryType_Nested_P2PK;

   auto& nestedScriptHash = getWitnessScriptH160();
   if (hashRef == nestedScriptHash)
      return AddressEntryType_Nested_P2WPKH;


   return AddressEntryType_Default;
}

////////////////////////////////////////////////////////////////////////////////
map<ScriptHashType, BinaryDataRef> AssetEntry_Single::getScriptHashMap() const
{
   map<ScriptHashType, BinaryDataRef> result;

   result.insert(make_pair(
      ScriptHash_P2PKH_Uncompressed, getHash160Uncompressed().getRef()));

   result.insert(make_pair(
      ScriptHash_P2PKH_Compressed, getHash160Compressed().getRef()));

   result.insert(make_pair(
      ScriptHash_P2WPKH, getWitnessScriptH160().getRef()));

   result.insert(make_pair(
      ScriptHash_Nested_P2PK, getP2PKScriptH160().getRef()));

   return result;
}

////////////////////////////////////////////////////////////////////////////////
const BinaryData& AssetEntry_Multisig::getScript() const
{
   if (multisigScript_.getSize() == 0)
   {
      BinaryWriter bw;

      //convert m to opcode and push
      auto m = m_ + OP_1 - 1;
      if (m > OP_16)
         throw WalletException("m exceeds OP_16");
      bw.put_uint8_t(m);

      //put pub keys
      for (auto& asset : assetMap_)
      {
         auto assetSingle =
            dynamic_pointer_cast<AssetEntry_Single>(asset.second);

         if (assetSingle == nullptr)
            WalletException("unexpected asset entry type");

         //using compressed keys
         auto& pubkeyCpr = assetSingle->getPubKey()->getCompressedKey();
         if (pubkeyCpr.getSize() != 33)
            throw WalletException("unexpected compress pub key len");

         bw.put_uint8_t(33);
         bw.put_BinaryData(pubkeyCpr);
      }

      //convert n to opcode and push
      auto n = n_ + OP_1 - 1;
      if (n > OP_16 || n < m)
         throw WalletException("invalid n");
      bw.put_uint8_t(n);

      bw.put_uint8_t(OP_CHECKMULTISIG);
      multisigScript_ = bw.getData();
   }

   return multisigScript_;
}

////////////////////////////////////////////////////////////////////////////////
const BinaryData& AssetEntry_Multisig::getHash160() const
{
   if (assetMap_.size() != n_)
      throw WalletException("asset count mismatch in multisig entry");

   if (h160_.getSize() == 0)
   {
      auto& msScript = getScript();
      h160_ = move(BtcUtils::getHash160(msScript));
   }

   return h160_;
}

////////////////////////////////////////////////////////////////////////////////
const BinaryData& AssetEntry_Multisig::getHash256() const
{
   if (assetMap_.size() != n_)
      throw WalletException("asset count mismatch in multisig entry");

   if (h256_.getSize() == 0)
   {
      auto& msScript = getScript();
      h256_ = move(BtcUtils::getSha256(msScript));
   }

   return h256_;
}

////////////////////////////////////////////////////////////////////////////////
const BinaryData& AssetEntry_Multisig::getP2WSHScript() const
{
   if (p2wshScript_.getSize() == 0)
   {
      auto& hash256 = getHash256();

      Recipient_PW2SH recipient(hash256, 0);
      auto& script = recipient.getSerializedScript();

      p2wshScript_ = move(script.getSliceCopy(9, script.getSize() - 9));
   }

   return p2wshScript_;
}

////////////////////////////////////////////////////////////////////////////////
const BinaryData& AssetEntry_Multisig::getP2WSHScriptH160() const
{
   if (p2wshScriptH160_.getSize() == 0)
   {
      auto& script = getP2WSHScript();

      p2wshScriptH160_ = move(BtcUtils::getHash160(script));
   }

   return p2wshScriptH160_;
}

////////////////////////////////////////////////////////////////////////////////
AddressEntryType AssetEntry_Multisig::getAddressTypeForHash(
   BinaryDataRef hashRef) const
{
   auto& nested = getP2WSHScriptH160();
   if (nested == hashRef)
      return AddressEntryType_Nested_P2WSH;

   auto& p2sh = getHash160();
   if (p2sh == hashRef)
      return AddressEntryType_Nested_Multisig;

   return AddressEntryType_Default;
}

////////////////////////////////////////////////////////////////////////////////
BinaryData AssetEntry::getDbKey() const
{
   BinaryWriter bw;
   bw.put_uint8_t(ASSETENTRY_PREFIX);
   bw.put_int32_t(index_);

   return bw.getData();
}

////////////////////////////////////////////////////////////////////////////////
bool AssetEntry::setAddressEntryType(AddressEntryType type)
{
   if (type == addressType_)
      return false;

   addressType_ = type;
   needsCommit_ = true;

   return true;
}

////////////////////////////////////////////////////////////////////////////////
shared_ptr<AssetEntry> AssetEntry::deserialize(
   BinaryDataRef key, BinaryDataRef value)
{
   BinaryRefReader brrKey(key);

   auto prefix = brrKey.get_uint8_t();
   if (prefix != ASSETENTRY_PREFIX)
      throw AssetDeserException("unexpected asset entry prefix");

   auto index = brrKey.get_int32_t();

   auto assetPtr = deserDBValue(index, value);
   assetPtr->doNotCommit();
   return assetPtr;
}

////////////////////////////////////////////////////////////////////////////////
shared_ptr<AssetEntry> AssetEntry::deserDBValue(int index, BinaryDataRef value)
{
   BinaryRefReader brrVal(value);
   auto val = brrVal.get_uint8_t();

   auto entryType = AssetEntryType(val & 0x0F);
   auto addressType = AddressEntryType((val & 0xF0) >> 4);

   switch (entryType)
   {
   case AssetEntryType_Single:
   {
      shared_ptr<Asset_PrivateKey> privKeyPtr = nullptr;

      SecureBinaryData pubKeyCompressed;
      SecureBinaryData pubKeyUncompressed;

      vector<pair<size_t, BinaryDataRef>> dataVec;

      while (brrVal.getSizeRemaining() > 0)
      {
         auto len = brrVal.get_var_int();
         auto valref = brrVal.get_BinaryDataRef(len);

         dataVec.push_back(make_pair(len, valref));
      }

      for (auto& datapair : dataVec)
      {
         BinaryRefReader brrData(datapair.second);
         auto keybyte = brrData.get_uint8_t();

         switch (keybyte)
         {
         case PUBKEY_UNCOMPRESSED_BYTE:
         {
            if (datapair.first != 66)
               throw AssetDeserException("invalid size for uncompressed pub key");

            if (pubKeyUncompressed.getSize() != 0)
               throw AssetDeserException("multiple pub keys for entry");

            pubKeyUncompressed = move(SecureBinaryData(
               brrData.get_BinaryDataRef(
               brrData.getSizeRemaining())));

            break;
         }

         case PUBKEY_COMPRESSED_BYTE:
         {
            if (datapair.first != 34)
               throw AssetDeserException("invalid size for compressed pub key");

            if (pubKeyCompressed.getSize() != 0)
               throw AssetDeserException("multiple pub keys for entry");

            pubKeyCompressed = move(SecureBinaryData(
               brrData.get_BinaryDataRef(
               brrData.getSizeRemaining())));

            break;
         }

         case PRIVKEY_BYTE:
         {
            if (privKeyPtr != nullptr)
               throw AssetDeserException("multiple priv keys for entry");

            privKeyPtr = dynamic_pointer_cast<Asset_PrivateKey>(
               Asset_EncryptedData::deserialize(datapair.first, datapair.second));

            if (privKeyPtr == nullptr)
               throw AssetDeserException("deserialized to unexpected type");
            break;
         }

         default:
            throw AssetDeserException("unknown key type byte");
         }
      }

      auto addrEntry = make_shared<AssetEntry_Single>(index, 
         pubKeyUncompressed, pubKeyCompressed, privKeyPtr);
      
      addrEntry->setAddressEntryType(addressType);
      addrEntry->doNotCommit();

      return addrEntry;
   }

   default:
      throw AssetDeserException("invalid asset entry type");
   }
}

////////////////////////////////////////////////////////////////////////////////
BinaryData AssetEntry_Single::serialize() const
{
   BinaryWriter bw;
   auto entryType = getType();
   auto addressType = getAddrType() << 4;
   bw.put_uint8_t(addressType | entryType);

   bw.put_BinaryData(pubkey_->serialize());
   if (privkey_ != nullptr && privkey_->hasData())
      bw.put_BinaryData(privkey_->serialize());
   
   BinaryWriter finalBw;

   finalBw.put_var_int(bw.getSize());
   finalBw.put_BinaryData(bw.getData());

   return finalBw.getData();
}

////////////////////////////////////////////////////////////////////////////////
bool AssetEntry_Single::hasPrivateKey() const
{
   if (privkey_ != nullptr)
      return privkey_->hasData();

   return false;
}

////////////////////////////////////////////////////////////////////////////////
const BinaryData& AssetEntry_Single::getPrivateEncryptionKeyId(void) const
{
   if (!hasPrivateKey())
      throw runtime_error("no private key in this asset");

   return privkey_->getEncryptionKeyID();
}

////////////////////////////////////////////////////////////////////////////////
bool AssetEntry_Multisig::hasPrivateKey() const
{  
   for (auto& asset_pair : assetMap_)
   {
      auto asset_single = 
         dynamic_pointer_cast<AssetEntry_Single>(asset_pair.second);
      if (asset_single == nullptr)
         throw runtime_error("unexpected asset entry type");

      if (!asset_single->hasPrivateKey())
         return false;
   }

   return true;
}

////////////////////////////////////////////////////////////////////////////////
const BinaryData& AssetEntry_Multisig::getPrivateEncryptionKeyId(void) const
{
   if (assetMap_.size() != n_)
      throw runtime_error("missing asset entries");

   if (!hasPrivateKey())
      throw runtime_error("no private key in this asset");

   map<BinaryData, const BinaryData&> idMap;

   for (auto& asset_pair : assetMap_)
   {
      auto asset_single =
         dynamic_pointer_cast<AssetEntry_Single>(asset_pair.second);
      if (asset_single == nullptr)
         throw runtime_error("unexpected asset entry type");

      idMap.insert(make_pair(
         asset_pair.first, asset_pair.second->getPrivateEncryptionKeyId()));
   }

   auto iditer = idMap.begin();
   auto& idref = iditer->second;
   ++iditer;

   while (iditer != idMap.end())
   {
      if (idref != iditer->second)
         throw runtime_error("wallets use different encryption keys");

      ++iditer;
   }

   return idref;
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
//// Cypher
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
Cypher::~Cypher()
{}

////////////////////////////////////////////////////////////////////////////////
unique_ptr<Cypher> Cypher::deserialize(BinaryRefReader& brr)
{
   unique_ptr<Cypher> cypher;
   auto prefix = brr.get_uint8_t();
   if (prefix != CYPHER_BYTE)
      throw runtime_error("invalid serialized cypher prefix");

   auto type = brr.get_uint8_t();

   auto len = brr.get_var_int();
   auto&& kdfId = brr.get_BinaryData(len);

   len = brr.get_var_int();
   auto&& encryptionKeyId = brr.get_BinaryData(len);

   len = brr.get_var_int();
   auto&& iv = SecureBinaryData(brr.get_BinaryDataRef(len));

   switch (type)
   {
   case CypherType_AES:
   {
      cypher = move(make_unique<Cypher_AES>(
         kdfId, encryptionKeyId, iv));

      break;
   }

   default:
      throw CypherException("unexpected cypher type");
   }

   return move(cypher);
}

////////////////////////////////////////////////////////////////////////////////
BinaryData Cypher_AES::serialize() const
{
   BinaryWriter bw;
   bw.put_uint8_t(CYPHER_BYTE);
   bw.put_uint8_t(getType());

   bw.put_var_int(kdfId_.getSize());
   bw.put_BinaryData(kdfId_);

   bw.put_var_int(encryptionKeyId_.getSize());
   bw.put_BinaryData(encryptionKeyId_);

   bw.put_var_int(iv_.getSize());
   bw.put_BinaryData(iv_);

   return bw.getData();
}

////////////////////////////////////////////////////////////////////////////////
unique_ptr<Cypher> Cypher_AES::getCopy() const
{
   return make_unique<Cypher_AES>(kdfId_, encryptionKeyId_);
}

////////////////////////////////////////////////////////////////////////////////
unique_ptr<Cypher> Cypher_AES::getCopy(const BinaryData& keyId) const
{
   return make_unique<Cypher_AES>(kdfId_, keyId);
}

////////////////////////////////////////////////////////////////////////////////
SecureBinaryData Cypher_AES::encrypt(const SecureBinaryData& key, 
   const SecureBinaryData& data) const
{
   CryptoAES aes_cypher;
   return aes_cypher.EncryptCBC(data, key, iv_);
}

////////////////////////////////////////////////////////////////////////////////
SecureBinaryData Cypher_AES::encrypt(DecryptedEncryptionKey* const key,
   const BinaryData& kdfId, const SecureBinaryData& data) const
{
   if (key == nullptr)
      throw runtime_error("empty ptr");

   auto& encryptionKey = key->getDerivedKey(kdfId);

   CryptoAES aes_cypher;
   return aes_cypher.EncryptCBC(data, encryptionKey, iv_);
}

////////////////////////////////////////////////////////////////////////////////
SecureBinaryData Cypher_AES::decrypt(const SecureBinaryData& key,
   const SecureBinaryData& data) const
{
   CryptoAES aes_cypher;
   return aes_cypher.DecryptCBC(data, key, iv_);
}

////////////////////////////////////////////////////////////////////////////////
bool Cypher_AES::isSame(Cypher* const cypher) const
{
   auto cypher_aes = dynamic_cast<Cypher_AES*>(cypher);
   if (cypher_aes == nullptr)
      return false;

   return kdfId_ == cypher_aes->kdfId_ &&
      encryptionKeyId_ == cypher_aes->encryptionKeyId_ &&
      iv_ == cypher_aes->iv_;
}
