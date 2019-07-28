#include "ZMQ_BIP15X_Helpers.h"

#include "EncryptionUtils.h"
#include "AuthorizedPeers.h"
#include "FutureValue.h"


ZmqBIP15XPeer::ZmqBIP15XPeer(const std::string &name, const BinaryData &pubKey)
   : name_(name)
   , pubKey_(pubKey)
{
   if (!ZmqBIP15XUtils::isValidPubKey(pubKey)) {
      throw std::runtime_error("invalid public key");
   }
}

// static
bool ZmqBIP15XUtils::isValidPubKey(const BinaryData &pubKey)
{
   // Based on CryptoECDSA::VerifyPublicKeyValid
   btc_pubkey key;
   btc_pubkey_init(&key);

   switch (pubKey.getSize()) {
      case BTC_ECKEY_COMPRESSED_LENGTH:
         key.compressed = true;
         break;
      case BTC_ECKEY_UNCOMPRESSED_LENGTH:
         key.compressed = false;
         break;
      default:
         return false;
   }

   std::memcpy(key.pubkey, pubKey.getPtr(), pubKey.getSize());
   return btc_pubkey_is_valid(&key);
}

bool ZmqBIP15XUtils::addAuthPeer(AuthorizedPeers *authPeers, const ZmqBIP15XPeer &peer)
{
   authPeers->eraseName(peer.name());
   authPeers->addPeer(peer.pubKey(), peer.name());
   return true;
}

void ZmqBIP15XUtils::updatePeerKeys(AuthorizedPeers *authPeers_, const std::vector<ZmqBIP15XPeer> &newPeers)
{
   // Make a copy of peers map!
   const auto oldPeers = authPeers_->getPeerNameMap();
   for (const auto &oldPeer : oldPeers) {
      // Own key pair is also stored here, we should preserve it
      if (oldPeer.first != "own") {
         authPeers_->eraseName(oldPeer.first);
      }
   }

   for (const auto &newPeer : newPeers) {
      authPeers_->addPeer(newPeer.pubKey(), newPeer.name());
   }
}
