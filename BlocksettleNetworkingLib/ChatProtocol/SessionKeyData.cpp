#include "ChatProtocol/SessionKeyData.h"

#include <disable_warnings.h>
#include <botan/bigint.h>
#include <botan/auto_rng.h>
#include <enable_warnings.h>
#include <utility>

namespace {
   constexpr size_t kDefaultNonceSize = 24;
}

using namespace Chat;

SessionKeyData::SessionKeyData(std::string userName)
   : userHash_(std::move(userName))
{

}

SessionKeyData::SessionKeyData(
   std::string userName, 
   BinaryData localSessionPublicKey,
   SecureBinaryData localSessionPrivateKey)
   : userHash_(std::move(userName)), 
   localSessionPublicKey_(std::move(localSessionPublicKey)), 
   localSessionPrivateKey_(std::move(localSessionPrivateKey))
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
   ++bigIntNonce;
   nonce_ = Botan::BigInt::encode_locked(bigIntNonce);
   return BinaryData(nonce_.data(), nonce_.size());
}

