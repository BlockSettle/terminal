////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//  Copyright (C) 2011-2015, Armory Technologies, Inc.                        //
//  Distributed under the GNU Affero General Public License (AGPL v3)         //
//  See LICENSE-ATI or http://www.gnu.org/licenses/agpl.html                  //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

#ifndef LIBBTC_ONLY
#include "EncryptionUtils.h"
#include "log.h"
#include "cryptopp/integer.h"
#include "cryptopp/oids.h"
#include "cryptopp/ripemd.h"

using namespace std;

/////////////////////////////////////////////////////////////////////////////
SecureBinaryData CryptoPRNG::generateRandom(uint32_t numBytes,
   SecureBinaryData extraEntropy)
{
   BTC_PRNG prng;

   // Entropy here refers to *EXTRA* entropy.  Crypto++ has it's own mechanism
   // for generating entropy which is sufficient, but it doesn't hurt to add
   // more if you have it.
   if (extraEntropy.getSize() > 0)
   {
      prng.IncorporateEntropy(
         (byte*)extraEntropy.getPtr(), extraEntropy.getSize());
   }

   SecureBinaryData randData(numBytes);
   prng.GenerateBlock(randData.getPtr(), numBytes);
   return randData;
}

/////////////////////////////////////////////////////////////////////////////
// Implement AES encryption using AES mode, CFB
SecureBinaryData CryptoAES::EncryptCFB(const SecureBinaryData & data, 
                                       const SecureBinaryData & key,
                                       const SecureBinaryData & iv)
{
   if(data.getSize() == 0)
      return SecureBinaryData(0);

   SecureBinaryData encrData(data.getSize());

   //sanity check
   if(iv.getSize() != BTC_AES::BLOCKSIZE)
      throw std::runtime_error("uninitialized IV!");

   BTC_CFB_MODE<BTC_AES>::Encryption aes_enc( (byte*)key.getPtr(), 
                                                     key.getSize(), 
                                              (byte*)iv.getPtr());

   aes_enc.ProcessData( (byte*)encrData.getPtr(), 
                        (byte*)data.getPtr(), 
                               data.getSize());

   return encrData;
}

/////////////////////////////////////////////////////////////////////////////
// Implement AES decryption using AES mode, CFB
SecureBinaryData CryptoAES::DecryptCFB(const SecureBinaryData & data, 
                                       const SecureBinaryData & key,
                                       const SecureBinaryData & iv  )
{
   if(data.getSize() == 0)
      return SecureBinaryData(0);

   SecureBinaryData unencrData(data.getSize());

   BTC_CFB_MODE<BTC_AES>::Decryption aes_enc( (byte*)key.getPtr(), 
                                                     key.getSize(), 
                                              (byte*)iv.getPtr());

   aes_enc.ProcessData( (byte*)unencrData.getPtr(), 
                        (byte*)data.getPtr(), 
                               data.getSize());

   return unencrData;
}

/////////////////////////////////////////////////////////////////////////////
// Same as above, but only changing the AES mode of operation (CBC, not CFB)
SecureBinaryData CryptoAES::EncryptCBC(const SecureBinaryData & data, 
                                       const SecureBinaryData & key,
                                       const SecureBinaryData & iv)
{
   if(data.getSize() == 0)
      return SecureBinaryData(0);

   SecureBinaryData encrData(data.getSize());

   //sanity check
   if(iv.getSize() != BTC_AES::BLOCKSIZE)
      throw std::runtime_error("uninitialized IV!");

   BTC_CBC_MODE<BTC_AES>::Encryption aes_enc( (byte*)key.getPtr(), 
                                                     key.getSize(), 
                                              (byte*)iv.getPtr());

   aes_enc.ProcessData( (byte*)encrData.getPtr(), 
                        (byte*)data.getPtr(), 
                               data.getSize());

   return encrData;
}

/////////////////////////////////////////////////////////////////////////////
// Same as above, but only changing the AES mode of operation (CBC, not CFB)
SecureBinaryData CryptoAES::DecryptCBC(const SecureBinaryData & data, 
                                       const SecureBinaryData & key,
                                       const SecureBinaryData & iv  )
{
   if(data.getSize() == 0)
      return SecureBinaryData(0);

   SecureBinaryData unencrData(data.getSize());

   BTC_CBC_MODE<BTC_AES>::Decryption aes_enc( (byte*)key.getPtr(), 
                                                     key.getSize(), 
                                              (byte*)iv.getPtr());

   aes_enc.ProcessData( (byte*)unencrData.getPtr(), 
                        (byte*)data.getPtr(), 
                               data.getSize());
   return unencrData;
}


/////////////////////////////////////////////////////////////////////////////
BTC_PRIVKEY CryptoECDSA::ParsePrivateKey(SecureBinaryData const & privKeyData)
{
   BTC_PRIVKEY cppPrivKey;

   CryptoPP::Integer privateExp;
   privateExp.Decode(privKeyData.getPtr(), privKeyData.getSize(), UNSIGNED);
   cppPrivKey.Initialize(CryptoPP::ASN1::secp256k1(), privateExp);
   return cppPrivKey;
}

/////////////////////////////////////////////////////////////////////////////
BTC_PUBKEY ParsePublicKey(SecureBinaryData const & pubKeyX32B,
   SecureBinaryData const & pubKeyY32B)
{
   BTC_PUBKEY cppPubKey;

   CryptoPP::Integer pubX;
   CryptoPP::Integer pubY;
   pubX.Decode(pubKeyX32B.getPtr(), pubKeyX32B.getSize(), UNSIGNED);
   pubY.Decode(pubKeyY32B.getPtr(), pubKeyY32B.getSize(), UNSIGNED);
   BTC_ECPOINT publicPoint(pubX, pubY);

   // Initialize the public key with the ECP point just created
   cppPubKey.Initialize(CryptoPP::ASN1::secp256k1(), publicPoint);

   // Validate the public key -- not sure why this needs a PRNG
   BTC_PRNG prng;
   assert(cppPubKey.Validate(prng, 3));

   return cppPubKey;
}

////////////////////////////////////////////////////////////////////////////////
CryptoPP::ECP Get_secp256k1_ECP(void)
{
   static bool firstRun = true;
   static CryptoPP::Integer intN;
   static CryptoPP::Integer inta;
   static CryptoPP::Integer intb;

   static BinaryData N;
   static BinaryData a;
   static BinaryData b;

   if (firstRun)
   {
      firstRun = false;
      N = BinaryData::CreateFromHex(
         "fffffffffffffffffffffffffffffffffffffffffffffffffffffffefffffc2f");
      a = BinaryData::CreateFromHex(
         "0000000000000000000000000000000000000000000000000000000000000000");
      b = BinaryData::CreateFromHex(
         "0000000000000000000000000000000000000000000000000000000000000007");

      intN.Decode(N.getPtr(), N.getSize(), UNSIGNED);
      inta.Decode(a.getPtr(), a.getSize(), UNSIGNED);
      intb.Decode(b.getPtr(), b.getSize(), UNSIGNED);
   }


   return CryptoPP::ECP(intN, inta, intb);
}

/////////////////////////////////////////////////////////////////////////////
BTC_PUBKEY ParsePublicKey(SecureBinaryData const & pubKey65B)
{
   BinaryDataRef bdr65 = pubKey65B.getRef();
   if (pubKey65B.getSize() == 33)
   {
      CryptoPP::ECP ecp = Get_secp256k1_ECP();
      BTC_ECPOINT ptPub;
      ecp.DecodePoint(ptPub, (byte*)pubKey65B.getPtr(), 33);
      SecureBinaryData ptUncompressed(65);
      ecp.EncodePoint((byte*)ptUncompressed.getPtr(), ptPub, false);

      BTC_PUBKEY cppPubKey;
      cppPubKey.Initialize(CryptoPP::ASN1::secp256k1(), ptPub);
      return cppPubKey;
   }
   SecureBinaryData pubXbin(pubKey65B.getSliceRef(1, 32));
   SecureBinaryData pubYbin(pubKey65B.getSliceRef(33, 32));
   return ParsePublicKey(pubXbin, pubYbin);
}

/////////////////////////////////////////////////////////////////////////////
SecureBinaryData SerializePublicKey(BTC_PUBKEY const & pubKey)
{
   BTC_ECPOINT publicPoint = pubKey.GetPublicElement();
   CryptoPP::Integer pubX = publicPoint.x;
   CryptoPP::Integer pubY = publicPoint.y;
   SecureBinaryData pubData(65);
   pubData.fill(0x04);  // we fill just to set the first byte...

   pubX.Encode(pubData.getPtr() + 1, 32, UNSIGNED);
   pubY.Encode(pubData.getPtr() + 33, 32, UNSIGNED);
   return pubData;
}
   
/////////////////////////////////////////////////////////////////////////////
SecureBinaryData CryptoECDSA::ComputePublicKey(
   SecureBinaryData const & cppPrivKey, bool compressed) const
{
   if (cppPrivKey.getSize() != 32)
      throw runtime_error("invalid priv key size");
   BTC_PRIVKEY pk = ParsePrivateKey(cppPrivKey);
   BTC_PUBKEY  pub;
   pk.MakePublicKey(pub);
   auto&& pubkey = SerializePublicKey(pub);
   if (compressed)
      pubkey = move(CompressPoint(pubkey));
   return pubkey;
}

/////////////////////////////////////////////////////////////////////////////
BTC_PUBKEY CryptoECDSA::ComputePublicKey(BTC_PRIVKEY const & cppPrivKey)
{
   BTC_PUBKEY  pub;
   cppPrivKey.MakePublicKey(pub);
   return pub;
}

////////////////////////////////////////////////////////////////////////////////
bool CheckPubPrivKeyMatch_CryptoPP(BTC_PRIVKEY const & cppPrivKey,
   BTC_PUBKEY  const & cppPubKey)
{
   BTC_PUBKEY computedPubKey;
   cppPrivKey.MakePublicKey(computedPubKey);

   BTC_ECPOINT ppA = cppPubKey.GetPublicElement();
   BTC_ECPOINT ppB = computedPubKey.GetPublicElement();
   return (ppA.x == ppB.x && ppA.y == ppB.y);
}

/////////////////////////////////////////////////////////////////////////////
bool CryptoECDSA::VerifyPublicKeyValid(SecureBinaryData const & pubKey)
{
   if(CRYPTO_DEBUG)
   {
      cout << "BinPub: " << pubKey.toHexStr() << endl;
   }

   SecureBinaryData keyToCheck(65);

   // To support compressed keys, we'll just check to see if a key is compressed
   // and then decompress it.
   if(pubKey.getSize() == 33) {
      keyToCheck = UncompressPoint(pubKey);
   }
   else {
      keyToCheck = pubKey;
   }

   // Basically just copying the ParsePublicKey method, but without
   // the assert that would throw an error from C++
   SecureBinaryData pubXbin(keyToCheck.getSliceRef( 1,32));
   SecureBinaryData pubYbin(keyToCheck.getSliceRef(33,32));
   CryptoPP::Integer pubX;
   CryptoPP::Integer pubY;
   pubX.Decode(pubXbin.getPtr(), pubXbin.getSize(), UNSIGNED);
   pubY.Decode(pubYbin.getPtr(), pubYbin.getSize(), UNSIGNED);
   BTC_ECPOINT publicPoint(pubX, pubY);

   // Initialize the public key with the ECP point just created
   BTC_PUBKEY cppPubKey;
   cppPubKey.Initialize(CryptoPP::ASN1::secp256k1(), publicPoint);

   // Validate the public key -- not sure why this needs a PRNG
   BTC_PRNG prng;
   return cppPubKey.Validate(prng, 3);
}

/////////////////////////////////////////////////////////////////////////////
// Use the secp256k1 curve to sign data of an arbitrary length.
// Input:  Data to sign  (const SecureBinaryData&)
//         The private key used to sign the data  (const BTC_PRIVKEY&)
//         A flag indicating if deterministic signing is used  (const bool&)
// Output: None
// Return: The signature of the data  (SecureBinaryData)
SecureBinaryData SignData_CryptoPP(SecureBinaryData const & binToSign, 
                                       BTC_PRIVKEY const & cppPrivKey,
                                       const bool& detSign)
{

   // We trick the Crypto++ ECDSA module by passing it a single-hashed
   // message, it will do the second hash before it signs it.  This is 
   // exactly what we need.
   CryptoPP::SHA256  sha256;
   BTC_PRNG prng;

   // Execute the first sha256 op -- the signer will do the other one
   SecureBinaryData hashVal(32);
   sha256.CalculateDigest(hashVal.getPtr(), 
                          binToSign.getPtr(), 
                          binToSign.getSize());

   // Do we want to use a PRNG or use deterministic signing (RFC 6979)?
   string signature;
   if(detSign)
   {
      BTC_DETSIGNER signer(cppPrivKey);
      CryptoPP::StringSource(
         hashVal.toBinStr(), true, new CryptoPP::SignerFilter(
         prng, signer, new CryptoPP::StringSink(signature)));
   }
   else
   {
      BTC_SIGNER signer(cppPrivKey);
      CryptoPP::StringSource(
         hashVal.toBinStr(), true, new CryptoPP::SignerFilter(
         prng, signer, new CryptoPP::StringSink(signature)));
   }

   return SecureBinaryData(signature);
}

/////////////////////////////////////////////////////////////////////////////
// Use the secp256k1 curve to sign data of an arbitrary length.
// Input:  Data to sign  (const SecureBinaryData&)
//         The private key used to sign the data  (const SecureBinaryData&)
//         A flag indicating if deterministic signing is used  (const bool&)
// Output: None
// Return: The signature of the data  (SecureBinaryData)
SecureBinaryData CryptoECDSA::SignData(SecureBinaryData const & binToSign,
   SecureBinaryData const & binPrivKey,
   const bool& detSign)
{
   if (CRYPTO_DEBUG)
   {
      cout << "SignData:" << endl;
      cout << "   BinSgn: " << binToSign.getSize() << " " << binToSign.toHexStr() << endl;
      cout << "   BinPrv: " << binPrivKey.getSize() << " " << binPrivKey.toHexStr() << endl;
      cout << "  DetSign: " << detSign << endl;
   }
   BTC_PRIVKEY cppPrivKey = ParsePrivateKey(binPrivKey);
   return SignData_CryptoPP(binToSign, cppPrivKey, detSign);
}

/////////////////////////////////////////////////////////////////////////////
bool VerifyData_CryptoPP(BinaryData const & binMessage,
   const BinaryData& sig,
   BTC_PUBKEY const & cppPubKey)
{
   /***
   This is the faster sig verification, with less sanity checks and copies.
   Meant for chain verifiation, use the SecureBinaryData versions for regular
   verifications.
   ***/

   CryptoPP::SHA256  sha256;
   BTC_PRNG prng;

   //pub keys are already validated by the script parser

   // We execute the first SHA256 op, here.  Next one is done by Verifier
   BinaryData hashVal(32);
   sha256.CalculateDigest(hashVal.getPtr(),
      binMessage.getPtr(),
      binMessage.getSize());

   // Verifying message 
   BTC_VERIFIER verifier(cppPubKey);
   return verifier.VerifyMessage((const byte*)hashVal.getPtr(),
      hashVal.getSize(),
      (const byte*)sig.getPtr(),
      sig.getSize());
}

/////////////////////////////////////////////////////////////////////////////
bool CryptoECDSA::VerifyData(BinaryData const & binMessage, 
                             BinaryData const & binSignature,
                             BinaryData const & pubkey65B) const
{
   if(CRYPTO_DEBUG)
   {
      cout << "VerifyData:" << endl;
      cout << "   BinMsg: " << binMessage.toHexStr() << endl;
      cout << "   BinSig: " << binSignature.toHexStr() << endl;
      cout << "   BinPub: " << pubkey65B.toHexStr() << endl;
   }

   BTC_PUBKEY cppPubKey = ParsePublicKey(pubkey65B);
   return VerifyData_CryptoPP(binMessage, binSignature, cppPubKey);
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


   // Hard-code the order of the group
   static SecureBinaryData SECP256K1_ORDER_BE = SecureBinaryData().CreateFromHex(
           "fffffffffffffffffffffffffffffffebaaedce6af48a03bbfd25e8cd0364141");
   
   CryptoPP::Integer mult, origPrivExp, ecOrder;
   // A 
   mult.Decode(chainXor.getPtr(), chainXor.getSize(), UNSIGNED);
   // B 
   origPrivExp.Decode(binPrivKey.getPtr(), binPrivKey.getSize(), UNSIGNED);
   // C
   ecOrder.Decode(SECP256K1_ORDER_BE.getPtr(), SECP256K1_ORDER_BE.getSize(), UNSIGNED);

   // A*B mod C will get us a new private key exponent
   CryptoPP::Integer newPrivExponent = 
                  a_times_b_mod_c(mult, origPrivExp, ecOrder);

   // Convert new private exponent to big-endian binary string 
   SecureBinaryData newPrivData(32);
   newPrivExponent.Encode(newPrivData.getPtr(), newPrivData.getSize(), UNSIGNED);

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
   static SecureBinaryData SECP256K1_ORDER_BE = SecureBinaryData::CreateFromHex(
           "fffffffffffffffffffffffffffffffebaaedce6af48a03bbfd25e8cd0364141");

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

   // Parse the chaincode as a big-endian integer
   CryptoPP::Integer mult;
   mult.Decode(chainXor.getPtr(), chainXor.getSize(), UNSIGNED);

   // "new" init as "old", to make sure it's initialized on the correct curve
   BTC_PUBKEY oldPubKey = ParsePublicKey(binPubKey); 
   BTC_PUBKEY newPubKey = ParsePublicKey(binPubKey);

   // Let Crypto++ do the EC math for us, serialize the new public key
   newPubKey.SetPublicElement( oldPubKey.ExponentiatePublicElement(mult) );

   if(multiplierOut != NULL)
      (*multiplierOut) = SecureBinaryData(chainXor);

   return SerializePublicKey(newPubKey);
}

////////////////////////////////////////////////////////////////////////////////
SecureBinaryData CryptoECDSA::InvMod(const SecureBinaryData& m)
{
   static BinaryData N = BinaryData::CreateFromHex(
           "fffffffffffffffffffffffffffffffebaaedce6af48a03bbfd25e8cd0364141");
   CryptoPP::Integer cppM;
   CryptoPP::Integer cppModulo;
   cppM.Decode(m.getPtr(), m.getSize(), UNSIGNED);
   cppModulo.Decode(N.getPtr(), N.getSize(), UNSIGNED);
   CryptoPP::Integer cppResult = cppM.InverseMod(cppModulo);
   SecureBinaryData result(32);
   cppResult.Encode(result.getPtr(), result.getSize(), UNSIGNED);
   return result;
}


////////////////////////////////////////////////////////////////////////////////
bool CryptoECDSA::ECVerifyPoint(BinaryData const & x,
                                BinaryData const & y)
{
   BTC_PUBKEY cppPubKey;

   CryptoPP::Integer pubX;
   CryptoPP::Integer pubY;
   pubX.Decode(x.getPtr(), x.getSize(), UNSIGNED);
   pubY.Decode(y.getPtr(), y.getSize(), UNSIGNED);
   BTC_ECPOINT publicPoint(pubX, pubY);

   // Initialize the public key with the ECP point just created
   cppPubKey.Initialize(CryptoPP::ASN1::secp256k1(), publicPoint);

   // Validate the public key -- not sure why this needs a PRNG
   BTC_PRNG prng;
   return cppPubKey.Validate(prng, 3);
}

////////////////////////////////////////////////////////////////////////////////
SecureBinaryData CryptoECDSA::CompressPoint(SecureBinaryData const & pubKey65)
{
   CryptoPP::ECP ecp = Get_secp256k1_ECP();
   BTC_ECPOINT ptPub;
   ecp.DecodePoint(ptPub, (byte*)pubKey65.getPtr(), 65);
   SecureBinaryData ptCompressed(33);
   ecp.EncodePoint((byte*)ptCompressed.getPtr(), ptPub, true);
   return ptCompressed; 
}

////////////////////////////////////////////////////////////////////////////////
SecureBinaryData CryptoECDSA::UncompressPoint(SecureBinaryData const & pubKey33)
{
   CryptoPP::ECP ecp = Get_secp256k1_ECP();
   BTC_ECPOINT ptPub;
   ecp.DecodePoint(ptPub, (byte*)pubKey33.getPtr(), 33);
   SecureBinaryData ptUncompressed(65);
   ecp.EncodePoint((byte*)ptUncompressed.getPtr(), ptPub, false);
   return ptUncompressed; 
}

////////////////////////////////////////////////////////////////////////////////
BinaryData CryptoECDSA::computeLowS(BinaryDataRef s)
{
   static SecureBinaryData SECP256K1_ORDER_BE = SecureBinaryData().CreateFromHex(
      "fffffffffffffffffffffffffffffffebaaedce6af48a03bbfd25e8cd0364141");

   CryptoPP::Integer ecOrder, sInteger;
   ecOrder.Decode(SECP256K1_ORDER_BE.getPtr(), SECP256K1_ORDER_BE.getSize(), UNSIGNED);

   //divide by 2
   auto&& halfOrder = ecOrder >> 1;

   sInteger.Decode(s.getPtr(), s.getSize(), UNSIGNED);
   if (sInteger > halfOrder)
      sInteger = ecOrder - sInteger;

   auto len = sInteger.ByteCount();
   BinaryData lowS(len);
   sInteger.Encode(lowS.getPtr(), len, UNSIGNED);

   return lowS;
}

////////////////////////////////////////////////////////////////////////////////
SecureBinaryData CryptoECDSA::PrivKeyScalarMultiply(
   const SecureBinaryData& privKey,
   const SecureBinaryData& scalar)
{
   static SecureBinaryData SECP256K1_ORDER_BE = SecureBinaryData().CreateFromHex(
      "fffffffffffffffffffffffffffffffebaaedce6af48a03bbfd25e8cd0364141");

   CryptoPP::Integer mult, origPrivExp, ecOrder;
   mult.Decode(scalar.getPtr(), scalar.getSize(), UNSIGNED);
   origPrivExp.Decode(privKey.getPtr(), privKey.getSize(), UNSIGNED);
   ecOrder.Decode(
      SECP256K1_ORDER_BE.getPtr(), SECP256K1_ORDER_BE.getSize(), UNSIGNED);

   // Let Crypto++ do the EC math for us, serialize the new public key
   CryptoPP::Integer newPrivExponent =
      a_times_b_mod_c(mult, origPrivExp, ecOrder);

   SecureBinaryData newPrivData(32);
   newPrivExponent.Encode(newPrivData.getPtr(), newPrivData.getSize(), UNSIGNED);

   return newPrivData;
}

////////////////////////////////////////////////////////////////////////////////
SecureBinaryData CryptoECDSA::PubKeyScalarMultiply(
   const SecureBinaryData& pubKey,
   const SecureBinaryData& scalar)
{
   // Parse the chaincode as a big-endian integer
   CryptoPP::Integer mult;
   mult.Decode(scalar.getPtr(), scalar.getSize(), UNSIGNED);

   // "new" init as "old", to make sure it's initialized on the correct curve
   BTC_PUBKEY oldPubKey;
   if (pubKey.getSize() == 33)
   {
       auto&& uncompressedKey = UncompressPoint(pubKey);
       oldPubKey = ParsePublicKey(uncompressedKey);
   }
   else
   {
      oldPubKey = ParsePublicKey(pubKey);
   }

   // Let Crypto++ do the EC math for us, serialize the new public key
   BTC_PUBKEY newPubKey;
   newPubKey.SetPublicElement(oldPubKey.ExponentiatePublicElement(mult));
   auto&& newPubKeySbd = SerializePublicKey(newPubKey);
   
   if (pubKey.getSize() == 33)
      return CompressPoint(newPubKeySbd);

   return newPubKeySbd;
}

////////////////////////////////////////////////////////////////////////////////
void CryptoSHA2::getHash256(BinaryDataRef bdr, uint8_t* digest)
{
   CryptoPP::SHA256  sha256;

   sha256.CalculateDigest(digest,
      bdr.getPtr(),
      bdr.getSize());
   sha256.CalculateDigest(digest,
      digest, 32);
}

////////////////////////////////////////////////////////////////////////////////
void CryptoSHA2::getSha256(BinaryDataRef bdr, uint8_t* digest)
{
   CryptoPP::SHA256  sha256;

   sha256.CalculateDigest(digest,
      bdr.getPtr(),
      bdr.getSize());
}

////////////////////////////////////////////////////////////////////////////////
void CryptoSHA2::getHMAC256(BinaryDataRef data, BinaryDataRef msg,
   uint8_t* digest)
{
   CryptoPP::HMAC<CryptoPP::SHA256> hmac(data.getPtr(), data.getSize());
   hmac.CalculateDigest(digest, (const byte*)msg.getPtr(), msg.getSize());
}

////////////////////////////////////////////////////////////////////////////////
void CryptoSHA2::getSha512(BinaryDataRef bdr, uint8_t* digest)
{
   CryptoPP::SHA512 sha512;
   
   sha512.CalculateDigest(digest,
      bdr.getPtr(),
      bdr.getSize());
}

////////////////////////////////////////////////////////////////////////////////
void CryptoSHA2::getHMAC512(BinaryDataRef data, BinaryDataRef msg,
   uint8_t* digest)
{
   CryptoPP::HMAC<CryptoPP::SHA512> hmac(data.getPtr(), data.getSize());
   hmac.CalculateDigest(digest, (const byte*)msg.getPtr(), msg.getSize());
}

////////////////////////////////////////////////////////////////////////////////
void CryptoHASH160::getHash160(BinaryDataRef bdr, uint8_t* digest)
{
   CryptoPP::RIPEMD160 ripemd160_;
   ripemd160_.CalculateDigest(digest, bdr.getPtr(), bdr.getSize());
}

#endif