#include "ChatProtocol/SessionKeyData.h"

#include <disable_warnings.h>
#include <botan/bigint.h>
#include <botan/base64.h>
#include <botan/auto_rng.h>
#include <enable_warnings.h>

namespace {
   constexpr size_t kDefaultNonceSize = 24;
}

using namespace Chat;

SessionKeyData::SessionKeyData(const std::string& userName)
   : userName_(userName)
{

}

SessionKeyData::SessionKeyData(const std::string& userName, const BinaryData& localSessionPublicKey, const SecureBinaryData& localSessionPrivateKey)
   : userName_(userName), localSessionPublicKey_(localSessionPublicKey), localSessionPrivateKey_(localSessionPrivateKey)
{

}

BinaryData SessionKeyData::nonce()
{
   if (nonce_.empty())
   {
      // generate new nonce
      Botan::AutoSeeded_RNG rng;
      nonce_ = rng.random_vec(kDefaultNonceSize);
      return BinaryData(nonce_.data(), nonce_.size());
   }

   // increment nonce 
   Botan::BigInt bigIntNonce;
   bigIntNonce.binary_decode(nonce_);
   bigIntNonce++;
   nonce_ = Botan::BigInt::encode_locked(bigIntNonce);
   return BinaryData(nonce_.data(), nonce_.size());
}

