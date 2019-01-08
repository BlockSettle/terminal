////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//  Copyright (C) 2019, goatpig                                               //
//  Distributed under the MIT license                                         //
//  See LICENSE-MIT or https://opensource.org/licenses/MIT                    //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////
#ifdef LIBBTC_ONLY
#include "EncryptionUtils.h"
#include "log.h"
#include "btc/ecc.h"
#include "btc/sha2.h"
#include "btc/ripemd160.h"

using namespace std;

#define CRYPTO_DEBUG false

/////////////////////////////////////////////////////////////////////////////
SecureBinaryData CryptoPRNG::generateRandom(uint32_t numBytes,
   SecureBinaryData extraEntropy)
{
   SecureBinaryData sbd(numBytes);
   btc_random_init();
   if (!btc_random_bytes(sbd.getPtr(), numBytes, 0))
      throw runtime_error("failed to generate random value");

   if (extraEntropy.getSize() != 0)
      sbd.XOR(extraEntropy);

   return sbd;
}

/////////////////////////////////////////////////////////////////////////////
// Implement AES encryption using AES mode, CFB
SecureBinaryData CryptoAES::EncryptCFB(SecureBinaryData & data, 
                                       SecureBinaryData & key,
                                       SecureBinaryData & iv)
{
   if(CRYPTO_DEBUG)
   {
      cout << "AES Decrypt" << endl;
      cout << "   BinData: " << data.toHexStr() << endl;
      cout << "   BinKey : " << key.toHexStr() << endl;
      cout << "   BinIV  : " << iv.toHexStr() << endl;
   }


   if(data.getSize() == 0)
      return SecureBinaryData(0);

   SecureBinaryData encrData(data.getSize());

   // Caller can supply their own IV/entropy, or let it be generated here
   // (variable "iv" is a reference, so check it on the way out)
   if(iv.getSize() == 0)
      iv = CryptoPRNG::generateRandom(16); //forced to aes256

   //set to cbc until i implement cbf in libbtc
   aes256_cbc_encrypt(
      key.getPtr(), iv.getPtr(), 
      data.getPtr(), data.getSize(), 
      1, //pad with 0s if the data is not aligned with 16 bytes blocks
      encrData.getPtr());

   return encrData;
}

/////////////////////////////////////////////////////////////////////////////
// Implement AES decryption using AES mode, CFB
SecureBinaryData CryptoAES::DecryptCFB(SecureBinaryData & data, 
                                       SecureBinaryData & key,
                                       SecureBinaryData   iv  )
{
   if(CRYPTO_DEBUG)
   {
      cout << "AES Decrypt" << endl;
      cout << "   BinData: " << data.toHexStr() << endl;
      cout << "   BinKey : " << key.toHexStr() << endl;
      cout << "   BinIV  : " << iv.toHexStr() << endl;
   }


   if(data.getSize() == 0)
      return SecureBinaryData(0);

   SecureBinaryData unencrData(data.getSize());

   aes256_cbc_decrypt(
      key.getPtr(), iv.getPtr(), 
      data.getPtr(), data.getSize(), 
      1, //data is padded to 16 bytes blocks
      unencrData.getPtr());

   return unencrData;
}

/////////////////////////////////////////////////////////////////////////////
// Same as above, but only changing the AES mode of operation (CBC, not CFB)
SecureBinaryData CryptoAES::EncryptCBC(const SecureBinaryData & data, 
                                       const SecureBinaryData & key,
                                       SecureBinaryData & iv) const
{
   if(CRYPTO_DEBUG)
   {
      cout << "AES Decrypt" << endl;
      cout << "   BinData: " << data.toHexStr() << endl;
      cout << "   BinKey : " << key.toHexStr() << endl;
      cout << "   BinIV  : " << iv.toHexStr() << endl;
   }

   if(data.getSize() == 0)
      return SecureBinaryData(0);

   size_t packet_count = data.getSize() / AES_BLOCK_SIZE + 1;

   SecureBinaryData encrData(packet_count * AES_BLOCK_SIZE);

   // Caller can supply their own IV/entropy, or let it be generated here
   // (variable "iv" is a reference, so check it on the way out)
   if (iv.getSize() == 0)
      iv = CryptoPRNG::generateRandom(16); //forced to aes256

   //set to cbc until i implement cbf in libbtc
   aes256_cbc_encrypt(
      key.getPtr(), iv.getPtr(),
      data.getPtr(), data.getSize(),
      1, //pad with 0s if the data is not aligned with 16 bytes blocks
      encrData.getPtr());

   return encrData;
}

/////////////////////////////////////////////////////////////////////////////
// Same as above, but only changing the AES mode of operation (CBC, not CFB)
SecureBinaryData CryptoAES::DecryptCBC(const SecureBinaryData & data, 
                                       const SecureBinaryData & key,
                                       const SecureBinaryData & iv  ) const
{
   if(CRYPTO_DEBUG)
   {
      cout << "AES Decrypt" << endl;
      cout << "   BinData: " << data.toHexStr() << endl;
      cout << "   BinKey : " << key.toHexStr() << endl;
      cout << "   BinIV  : " << iv.toHexStr() << endl;
   }

   if(data.getSize() == 0)
      return SecureBinaryData(0);

   SecureBinaryData unencrData(data.getSize());

   auto size = aes256_cbc_decrypt(
      key.getPtr(), iv.getPtr(),
      data.getPtr(), data.getSize(),
      1, //data is padded to 16 bytes blocks
      unencrData.getPtr());

   if (size == 0)
      throw runtime_error("failed to decrypt packet");

   if (size < unencrData.getSize())
      unencrData.resize(size);

   return unencrData;
}

/////////////////////////////////////////////////////////////////////////////
SecureBinaryData CryptoECDSA::ComputePublicKey(
   SecureBinaryData const & cppPrivKey) const
{
   btc_key pkey;
   btc_privkey_init(&pkey);
   memcpy(pkey.privkey, cppPrivKey.getPtr(), 32);
   btc_privkey_is_valid(&pkey);

   btc_pubkey pubkey;
   SecureBinaryData result(BTC_ECKEY_UNCOMPRESSED_LENGTH);

   btc_pubkey_init(&pubkey);
   btc_pubkey_from_key(&pkey, &pubkey);
   memcpy(result.getPtr(), pubkey.pubkey, BTC_ECKEY_UNCOMPRESSED_LENGTH);

   return result;
}

////////////////////////////////////////////////////////////////////////////////
bool CryptoECDSA::VerifyPublicKeyValid(SecureBinaryData const & pubKey)
{
   if(CRYPTO_DEBUG)
   {
      cout << "BinPub: " << pubKey.toHexStr() << endl;
   }

   btc_pubkey key;
   btc_pubkey_init(&key);
   memcpy(key.pubkey, pubKey.getPtr(), pubKey.getSize());
   key.compressed = pubKey.getSize() == 33 ? true : false;
   return btc_pubkey_is_valid(&key);
}

/////////////////////////////////////////////////////////////////////////////
// Use the secp256k1 curve to sign data of an arbitrary length.
// Input:  Data to sign  (const SecureBinaryData&)
//         The private key used to sign the data  (const BTC_PRIVKEY&)
//         A flag indicating if deterministic signing is used  (const bool&)
// Output: None
// Return: The signature of the data  (SecureBinaryData)
SecureBinaryData CryptoECDSA::SignData(SecureBinaryData const & binToSign, 
   SecureBinaryData const & cppPrivKey, const bool& detSign)
{

   // We trick the Crypto++ ECDSA module by passing it a single-hashed
   // message, it will do the second hash before it signs it.  This is 
   // exactly what we need.

   //hash message
   SecureBinaryData digest1(32), digest2(32);
   sha256_Raw(binToSign.getPtr(), binToSign.getSize(), digest1.getPtr());
   sha256_Raw(digest1.getPtr(), 32, digest2.getPtr());

   // Only use RFC 6979
   SecureBinaryData sig(74);
   size_t outlen = 74;

   btc_key pkey;
   btc_privkey_init(&pkey);
   memcpy(pkey.privkey, cppPrivKey.getPtr(), 32);

   btc_key_sign_hash(&pkey, digest2.getPtr(), sig.getPtr(), &outlen);
   if(outlen != 74)
      sig.resize(outlen);

   return sig;
}

/////////////////////////////////////////////////////////////////////////////
bool CryptoECDSA::VerifyData(BinaryData const & binMessage,
   const BinaryData& sig,
   BinaryData const & cppPubKey) const
{
   /***
   This is the faster sig verification, with less sanity checks and copies.
   Meant for chain verifiation, use the SecureBinaryData versions for regular
   verifications.
   ***/

   //pub keys are already validated by the script parser

   // We execute the first SHA256 op, here.  Next one is done by Verifier
   BinaryData digest1(32), digest2(32);
   sha256_Raw(binMessage.getPtr(), binMessage.getSize(), digest1.getPtr());
   sha256_Raw(digest1.getPtr(), 32, digest2.getPtr());

   //setup pubkey
   btc_pubkey key;
   btc_pubkey_init(&key);
   memcpy(key.pubkey, cppPubKey.getPtr(), cppPubKey.getSize());
   key.compressed = cppPubKey.getSize() == 33 ? true : false;

   // Verifying message 
   return btc_pubkey_verify_sig(
      &key, digest2.getPtr(),
      (unsigned char*)sig.getCharPtr(), sig.getSize());
}

/////////////////////////////////////////////////////////////////////////////
// Deterministically generate new private key using a chaincode
// Changed:  added using the hash of the public key to the mix
//           b/c multiplying by the chaincode alone is too "linear"
//           (there's no reason to believe it's insecure, but it doesn't
//           hurt to add some extra entropy/non-linearity to the chain
//           generation process)
SecureBinaryData CryptoECDSA::ComputeChainedPrivateKey(
                                 SecureBinaryData const & binPrivKey,
                                 SecureBinaryData const & chainCode,
                                 SecureBinaryData* multiplierOut)
{
   auto&& binPubKey = ComputePublicKey(binPrivKey);

   if( binPrivKey.getSize() != 32 || chainCode.getSize() != 32)
   {
      LOGERR << "***ERROR:  Invalid private key or chaincode (both must be 32B)";
      LOGERR << "BinPrivKey: " << binPrivKey.getSize();
      LOGERR << "BinPrivKey: (not logged for security)";
      LOGERR << "BinChain  : " << chainCode.getSize();
      LOGERR << "BinChain  : " << chainCode.toHexStr();
   }

   // Adding extra entropy to chaincode by xor'ing with hash256 of pubkey
   BinaryData chainMod  = binPubKey.getHash256();
   BinaryData chainOrig = chainCode.getRawCopy();
   BinaryData chainXor(32);
      
   for(uint8_t i=0; i<8; i++)
   {
      uint8_t offset = 4*i;
      *(uint32_t*)(chainXor.getPtr()+offset) =
                           *(uint32_t*)( chainMod.getPtr()+offset) ^ 
                           *(uint32_t*)(chainOrig.getPtr()+offset);
   }

   SecureBinaryData newPrivData(binPrivKey);
   if (!btc_ecc_private_key_tweak_mul((uint8_t*)newPrivData.getPtr(), chainXor.getPtr()))
      throw runtime_error("failed to multiply priv key");

   if(multiplierOut != NULL)
      (*multiplierOut) = SecureBinaryData(chainXor);

   return newPrivData;
}
                            
/////////////////////////////////////////////////////////////////////////////
// Deterministically generate new public key using a chaincode
SecureBinaryData CryptoECDSA::ComputeChainedPublicKey(
                                SecureBinaryData const & binPubKey,
                                SecureBinaryData const & chainCode,
                                SecureBinaryData* multiplierOut)
{
   if(CRYPTO_DEBUG)
   {
      cout << "ComputeChainedPUBLICKey:" << endl;
      cout << "   BinPub: " << binPubKey.toHexStr() << endl;
      cout << "   BinChn: " << chainCode.toHexStr() << endl;
   }

   // Added extra entropy to chaincode by xor'ing with hash256 of pubkey
   BinaryData chainMod  = binPubKey.getHash256();
   BinaryData chainOrig = chainCode.getRawCopy();
   BinaryData chainXor(32);
      
   for(uint8_t i=0; i<8; i++)
   {
      uint8_t offset = 4*i;
      *(uint32_t*)(chainXor.getPtr()+offset) =
                           *(uint32_t*)( chainMod.getPtr()+offset) ^ 
                           *(uint32_t*)(chainOrig.getPtr()+offset);
   }

   SecureBinaryData pubKeyResult(binPubKey);
   if (!btc_ecc_public_key_tweak_mul((uint8_t*)pubKeyResult.getPtr(), chainXor.getPtr()))
      throw runtime_error("failed to multiply pubkey");

   if(multiplierOut != NULL)
      (*multiplierOut) = SecureBinaryData(chainXor);

   return pubKeyResult;
}

////////////////////////////////////////////////////////////////////////////////
bool CryptoECDSA::ECVerifyPoint(BinaryData const & x,
                                BinaryData const & y)
{
   BinaryWriter bw;
   bw.put_uint8_t(4);
   bw.put_BinaryData(x);
   bw.put_BinaryData(y);
   auto ptr = bw.getDataRef().getPtr();

   return btc_ecc_verify_pubkey(ptr, false);
}

////////////////////////////////////////////////////////////////////////////////
SecureBinaryData CryptoECDSA::CompressPoint(SecureBinaryData const & pubKey65)
{
   SecureBinaryData ptCompressed(33);
   if (!btc_ecc_public_key_compress(
      (uint8_t*)pubKey65.getPtr(), ptCompressed.getPtr()))
   {
      ptCompressed = pubKey65;
   }

   return ptCompressed; 
}

////////////////////////////////////////////////////////////////////////////////
SecureBinaryData CryptoECDSA::UncompressPoint(SecureBinaryData const & pubKey33)
{
   SecureBinaryData ptUncompressed(65);
   if(!btc_ecc_public_key_uncompress((uint8_t*)pubKey33.getPtr(), ptUncompressed.getPtr()))
      ptUncompressed = pubKey33;

   return ptUncompressed; 
}

////////////////////////////////////////////////////////////////////////////////
void CryptoSHA2::getHash256(BinaryDataRef bdr, uint8_t* digest)
{
   sha256_Raw(bdr.getPtr(), bdr.getSize(), digest);
   sha256_Raw(digest, 32, digest);
}

////////////////////////////////////////////////////////////////////////////////
void CryptoSHA2::getSha256(BinaryDataRef bdr, uint8_t* digest)
{
   sha256_Raw(bdr.getPtr(), bdr.getSize(), digest);
}

////////////////////////////////////////////////////////////////////////////////
void CryptoSHA2::getHMAC256(BinaryDataRef data, BinaryDataRef msg, 
   uint8_t* digest)
{
   hmac_sha256(data.getPtr(), data.getSize(), msg.getPtr(), msg.getSize(), 
      digest);
}

////////////////////////////////////////////////////////////////////////////////
void CryptoSHA2::getSha512(BinaryDataRef bdr, uint8_t* digest)
{
   sha512_Raw(bdr.getPtr(), bdr.getSize(), digest);
}

////////////////////////////////////////////////////////////////////////////////
void CryptoSHA2::getHMAC512(BinaryDataRef data, BinaryDataRef msg,
   uint8_t* digest)
{
   hmac_sha512(data.getPtr(), data.getSize(), msg.getPtr(), msg.getSize(),
      digest);
}

////////////////////////////////////////////////////////////////////////////////
void CryptoHASH160::getHash160(BinaryDataRef bdr, uint8_t* digest)
{
   btc_ripemd160(bdr.getPtr(), bdr.getSize(), digest);
}

#endif