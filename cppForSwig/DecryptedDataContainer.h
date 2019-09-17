////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//  Copyright (C) 2017, goatpig                                               //
//  Distributed under the MIT license                                         //
//  See LICENSE-MIT or https://opensource.org/licenses/MIT                    //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

#ifndef _H_DECRYPTED_DATA_CONTAINER
#define _H_DECRYPTED_DATA_CONTAINER

#include <functional>
#include "Assets.h"
#include "ReentrantLock.h"
#include "BinaryData.h"
#include "lmdbpp.h"

#define ENCRYPTIONKEY_PREFIX        0xC0
#define ENCRYPTIONKEY_PREFIX_TEMP   0xCC

class AssetUnavailableException
{};

class DecryptedDataContainerException : public std::runtime_error
{
public:
   DecryptedDataContainerException(const std::string& msg) : std::runtime_error(msg)
   {}
};

class EncryptedDataMissing : public std::runtime_error
{
public:
   EncryptedDataMissing() : std::runtime_error("")
   {}
};

////////////////////////////////////////////////////////////////////////////////
class DecryptedDataContainer : public Lockable
{
   struct DecryptedDataMaps
   {
      std::map<BinaryData, std::unique_ptr<DecryptedEncryptionKey>> encryptionKeys_;
      std::map<BinaryData, std::unique_ptr<DecryptedData>> privateData_;
   };

private:
   std::map<BinaryData, std::shared_ptr<KeyDerivationFunction>> kdfMap_;
   std::unique_ptr<DecryptedDataMaps> lockedDecryptedData_ = nullptr;

   struct OtherLockedContainer
   {
      std::shared_ptr<DecryptedDataContainer> container_;
      std::shared_ptr<ReentrantLock> lock_;

      OtherLockedContainer(std::shared_ptr<DecryptedDataContainer> obj)
      {
         if (obj == nullptr)
            throw std::runtime_error("emtpy DecryptedDataContainer ptr");

         lock_ = make_unique<ReentrantLock>(obj.get());
      }
   };

   std::vector<OtherLockedContainer> otherLocks_;

   LMDBEnv* dbEnv_;
   LMDB* dbPtr_;

   /*
   The default encryption key is used to encrypt the master encryption in
   case no passphrase was provided at wallet creation. This is to prevent
   for the master key being written in plain text on disk. It is encryption
   but does not effectively result in the wallet being protected by encryption,
   since the default encryption key is written on disk in plain text.

   This is mostly to allow for the entire container to be encrypted head to toe
   without implementing large caveats to handle unencrypted use cases.
   */
   const SecureBinaryData defaultEncryptionKey_;
   const SecureBinaryData defaultEncryptionKeyId_;
   
   const SecureBinaryData defaultKdfId_;
   const SecureBinaryData masterEncryptionKeyId_;

protected:
   std::map<BinaryData, std::shared_ptr<Asset_EncryptedData>> encryptionKeyMap_;

private:
   std::function<SecureBinaryData(
      const std::set<BinaryData>&)> getPassphraseLambda_;

private:
   std::unique_ptr<DecryptedEncryptionKey> deriveEncryptionKey(
      std::unique_ptr<DecryptedEncryptionKey>, const BinaryData& kdfid) const;

   std::unique_ptr<DecryptedEncryptionKey> promptPassphrase(
      const std::map<BinaryData, BinaryData>&) const;

   void initAfterLock(void);
   void cleanUpBeforeUnlock(void);

   const SecureBinaryData& getDefaultEncryptionKeyId(void) const
   {
      return defaultEncryptionKeyId_;
   }

public:
   DecryptedDataContainer(LMDBEnv* dbEnv, LMDB* dbPtr,
      const SecureBinaryData& defaultEncryptionKey,
      const BinaryData& defaultEncryptionKeyId,
      const SecureBinaryData& defaultKdfId,
      const SecureBinaryData& masterKeyId) :
      dbEnv_(dbEnv), dbPtr_(dbPtr),
      defaultEncryptionKey_(defaultEncryptionKey),
      defaultEncryptionKeyId_(defaultEncryptionKeyId),
      defaultKdfId_(defaultKdfId),
      masterEncryptionKeyId_(masterKeyId)
   {
      resetPassphraseLambda();
   }

   const SecureBinaryData& getDecryptedPrivateData(
      std::shared_ptr<Asset_EncryptedData> data);
   SecureBinaryData encryptData(
      Cipher* const cipher, const SecureBinaryData& data);


   void populateEncryptionKey(const std::map<BinaryData, BinaryData>&);

   void addKdf(std::shared_ptr<KeyDerivationFunction> kdfPtr)
   {
      kdfMap_.insert(std::make_pair(kdfPtr->getId(), kdfPtr));
   }

   void addEncryptionKey(std::shared_ptr<Asset_EncryptionKey> keyPtr)
   {
      encryptionKeyMap_.insert(std::make_pair(keyPtr->getId(), keyPtr));
   }

   void updateOnDisk(void);
   void readFromDisk(void);

   void updateKeyOnDiskNoPrefix(
      const BinaryData&, std::shared_ptr<Asset_EncryptedData>);
   void updateKeyOnDisk(
      const BinaryData&, std::shared_ptr<Asset_EncryptedData>);

   void deleteKeyFromDisk(const BinaryData& key);

   void setPassphrasePromptLambda(
      std::function<SecureBinaryData(const std::set<BinaryData>&)> lambda)
   {
      getPassphraseLambda_ = lambda;
   }

   void resetPassphraseLambda(void) { getPassphraseLambda_ = nullptr; }

   void encryptEncryptionKey(
      const BinaryData& keyID, const BinaryData& kdfID, 
      const SecureBinaryData& newPassphrase, bool replace = true);

   void lockOther(std::shared_ptr<DecryptedDataContainer> other);

   const SecureBinaryData& getDefaultKdfId(void) const { return defaultKdfId_; }
   const SecureBinaryData& getMasterEncryptionKeyId(void) const
   {
      return masterEncryptionKeyId_;
   }

};

#endif
