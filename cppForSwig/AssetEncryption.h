////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//  Copyright (C) 2017, goatpig                                               //
//  Distributed under the MIT license                                         //
//  See LICENSE-MIT or https://opensource.org/licenses/MIT                    //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

#ifndef _H_ASSET_ENCRYPTION
#define _H_ASSET_ENCRYPTION

#include <memory>
#include "BinaryData.h"
#include "EncryptionUtils.h"

#define KDF_PREFIX                  0xC1
#define KDF_ROMIX_PREFIX            0xC100
#define CYPHER_BYTE                 0xB2

enum CypherType
{
   CypherType_AES,
   CypherType_Serpent
};

class CypherException : public std::runtime_error
{
public:
   CypherException(const std::string& msg) : std::runtime_error(msg)
   {}
};

////////////////////////////////////////////////////////////////////////////////
struct KeyDerivationFunction
{
private:

public:
   KeyDerivationFunction(void)
   {}

   virtual ~KeyDerivationFunction(void) = 0;
   virtual SecureBinaryData deriveKey(
      const SecureBinaryData& rawKey) const = 0;
   virtual bool isSame(KeyDerivationFunction* const) const = 0;

   bool operator<(const KeyDerivationFunction& rhs)
   {
      return getId() < rhs.getId();
   }

   virtual const BinaryData& getId(void) const = 0;
   virtual BinaryData serialize(void) const = 0;
   static std::shared_ptr<KeyDerivationFunction>
      deserialize(const BinaryDataRef&);
};

////////////////////////////////////////////////////////////////////////////////
struct KeyDerivationFunction_Romix : public KeyDerivationFunction
{
private:
   mutable BinaryData id_;
   unsigned iterations_;
   unsigned memTarget_;
   const BinaryData salt_;

private:
   BinaryData computeID(void) const;
   BinaryData initialize(void);

public:
   KeyDerivationFunction_Romix() :
      KeyDerivationFunction(),
      salt_(std::move(initialize()))
   {}

   KeyDerivationFunction_Romix(unsigned iterations, unsigned memTarget,
      SecureBinaryData& salt) :
      KeyDerivationFunction(),
      iterations_(iterations), memTarget_(memTarget), salt_(salt)
   {}

   SecureBinaryData deriveKey(const SecureBinaryData& rawKey) const;
   bool isSame(KeyDerivationFunction* const) const;
   BinaryData serialize(void) const;
   const BinaryData& getId(void) const;
};

struct DecryptedEncryptionKey;

////////////////////////////////////////////////////////////////////////////////
class Cypher
{
private:
   const CypherType type_;

protected:
   const BinaryData kdfId_;
   const BinaryData encryptionKeyId_;
   mutable SecureBinaryData iv_;

public:

   //tors
   Cypher(CypherType type, const BinaryData& kdfId,
      const BinaryData encryptionKeyId) :
      type_(type), kdfId_(kdfId), encryptionKeyId_(encryptionKeyId)
   {}

   virtual ~Cypher(void) = 0;

   //locals
   CypherType getType(void) const { return type_; }
   const BinaryData& getKdfId(void) const { return kdfId_; }
   const BinaryData& getEncryptionKeyId(void) const { return encryptionKeyId_; }
   const SecureBinaryData& getIV(void) const { return iv_; }

   //virtuals
   virtual BinaryData serialize(void) const = 0;
   virtual std::unique_ptr<Cypher> getCopy(void) const = 0;
   virtual std::unique_ptr<Cypher> getCopy(const BinaryData& keyId) const = 0;

   virtual SecureBinaryData encrypt(const SecureBinaryData& key,
      const SecureBinaryData& data) const = 0;
   virtual SecureBinaryData encrypt(DecryptedEncryptionKey* const key,
      const BinaryData& kdfId, const SecureBinaryData& data) const = 0;

   virtual SecureBinaryData decrypt(const SecureBinaryData& key,
      const SecureBinaryData& data) const = 0;

   virtual bool isSame(Cypher* const) const = 0;

   //statics
   static std::unique_ptr<Cypher> deserialize(BinaryRefReader& brr);
};

////////////////////////////////////////////////////////////////////////////////
class Cypher_AES : public Cypher
{
public:
   //tors
   Cypher_AES(const BinaryData& kdfId, const BinaryData& encryptionKeyId) :
      Cypher(CypherType_AES, kdfId, encryptionKeyId)
   {
      //init IV
      iv_ = std::move(SecureBinaryData().GenerateRandom(BTC_AES::BLOCKSIZE));
   }

   Cypher_AES(const BinaryData& kdfId, const BinaryData& encryptionKeyId,
      SecureBinaryData& iv) :
      Cypher(CypherType_AES, kdfId, encryptionKeyId)
   {
      if (iv.getSize() != BTC_AES::BLOCKSIZE)
         throw CypherException("invalid iv length");

      iv_ = std::move(iv);
   }

   //virtuals
   BinaryData serialize(void) const;
   std::unique_ptr<Cypher> getCopy(void) const;
   std::unique_ptr<Cypher> getCopy(const BinaryData& keyId) const;

   SecureBinaryData encrypt(const SecureBinaryData& key,
      const SecureBinaryData& data) const;
   SecureBinaryData encrypt(DecryptedEncryptionKey* const key,
      const BinaryData& kdfId, const SecureBinaryData& data) const;


   SecureBinaryData decrypt(const SecureBinaryData& key,
      const SecureBinaryData& data) const;

   bool isSame(Cypher* const) const;
};

#endif