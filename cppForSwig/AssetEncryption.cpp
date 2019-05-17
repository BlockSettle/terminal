////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//  Copyright (C) 2016, goatpig                                               //
//  Distributed under the MIT license                                         //
//  See LICENSE-MIT or https://opensource.org/licenses/MIT                    //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

#include "AssetEncryption.h"
#include "Assets.h"
#include "make_unique.h"

using namespace std;

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

   BinaryData bd(32);
   CryptoSHA2::getHash256(bw.getData(), bd.getPtr());
   return bd;
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
//// Cipher
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
Cipher::~Cipher()
{}

////////////////////////////////////////////////////////////////////////////////
unsigned Cipher::getBlockSize(CipherType type)
{
   unsigned blockSize;
   switch (type)
   {
   case CipherType_AES:
   {
#ifdef LIBBTC_ONLY
      blockSize = AES_BLOCK_SIZE;
#else
      blockSize = BTC_AES::BLOCKSIZE;
#endif
      break;
   }

   default:
      throw runtime_error("cannot get block size for unexpected cipher type");
   }

   return blockSize;
}

////////////////////////////////////////////////////////////////////////////////
SecureBinaryData Cipher::generateIV(void) const
{
   return CryptoPRNG::generateRandom(getBlockSize(type_));
}

////////////////////////////////////////////////////////////////////////////////
unique_ptr<Cipher> Cipher::deserialize(BinaryRefReader& brr)
{
   unique_ptr<Cipher> cipher;
   auto prefix = brr.get_uint8_t();
   if (prefix != CIPHER_BYTE)
      throw runtime_error("invalid serialized cipher prefix");

   auto type = brr.get_uint8_t();

   auto len = brr.get_var_int();
   auto&& kdfId = brr.get_BinaryData(len);

   len = brr.get_var_int();
   auto&& encryptionKeyId = brr.get_BinaryData(len);

   len = brr.get_var_int();
   auto&& iv = SecureBinaryData(brr.get_BinaryDataRef(len));

   switch (type)
   {
   case CipherType_AES:
   {
      cipher = move(make_unique<Cipher_AES>(
         kdfId, encryptionKeyId, iv));

      break;
   }

   default:
      throw CipherException("unexpected cipher type");
   }

   return move(cipher);
}

////////////////////////////////////////////////////////////////////////////////
BinaryData Cipher_AES::serialize() const
{
   BinaryWriter bw;
   bw.put_uint8_t(CIPHER_BYTE);
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
unique_ptr<Cipher> Cipher_AES::getCopy() const
{
   return make_unique<Cipher_AES>(kdfId_, encryptionKeyId_);
}

////////////////////////////////////////////////////////////////////////////////
unique_ptr<Cipher> Cipher_AES::getCopy(const BinaryData& keyId) const
{
   return make_unique<Cipher_AES>(kdfId_, keyId);
}

////////////////////////////////////////////////////////////////////////////////
SecureBinaryData Cipher_AES::encrypt(const SecureBinaryData& key,
   const SecureBinaryData& data) const
{
   CryptoAES aes_cipher;
   return aes_cipher.EncryptCBC(data, key, iv_);
}

////////////////////////////////////////////////////////////////////////////////
SecureBinaryData Cipher_AES::encrypt(DecryptedEncryptionKey* const key,
   const BinaryData& kdfId, const SecureBinaryData& data) const
{
   if (key == nullptr)
      throw runtime_error("empty ptr");

   auto& encryptionKey = key->getDerivedKey(kdfId);

   CryptoAES aes_cipher;
   return aes_cipher.EncryptCBC(data, encryptionKey, iv_);
}

////////////////////////////////////////////////////////////////////////////////
SecureBinaryData Cipher_AES::decrypt(const SecureBinaryData& key,
   const SecureBinaryData& data) const
{
   CryptoAES aes_cipher;
   return aes_cipher.DecryptCBC(data, key, iv_);
}

////////////////////////////////////////////////////////////////////////////////
bool Cipher_AES::isSame(Cipher* const cipher) const
{
   auto cipher_aes = dynamic_cast<Cipher_AES*>(cipher);
   if (cipher_aes == nullptr)
      return false;

   return kdfId_ == cipher_aes->kdfId_ &&
      encryptionKeyId_ == cipher_aes->encryptionKeyId_ &&
      iv_ == cipher_aes->iv_;
}

////////////////////////////////////////////////////////////////////////////////
unsigned Cipher_AES::getBlockSize(void) const
{
   return Cipher::getBlockSize(getType());
}