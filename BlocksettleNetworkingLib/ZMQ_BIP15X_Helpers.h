#ifndef ZMQ_BIP15X_HELPERS_H
#define ZMQ_BIP15X_HELPERS_H

#include <functional>
#include <future>
#include <string>
#include "BinaryData.h"

class AuthorizedPeers;
template<typename T> class FutureValue;

// Immutable ZMQ BIP15X peer public key, guaranteed to be valid (no need to check pubKey over and over)
class ZmqBIP15XPeer
{
public:
   // Will throw if pubKey is invalid
   ZmqBIP15XPeer(const std::string &name, const BinaryData &pubKey);

   const std::string &name() const { return name_; }
   const BinaryData &pubKey() const { return pubKey_; }
private:
   // key name
   const std::string name_;

   // EC public key (33 bytes if compressed)
   const BinaryData pubKey_;
};

using ZmqBIP15XPeers = std::vector<ZmqBIP15XPeer>;

class ZmqBIP15XUtils
{
public:
   static bool isValidPubKey(const BinaryData &pubKey);

   static bool addAuthPeer(AuthorizedPeers *authPeers, const ZmqBIP15XPeer &peer);

   static void updatePeerKeys(AuthorizedPeers *authPeers, const std::vector<ZmqBIP15XPeer> &peers);
};

using ZmqBipNewKeyCb = std::function<void(const std::string &oldKey, const std::string &newKey
   , const std::string& srvAddrPort, const std::shared_ptr<FutureValue<bool>> &prompt)>;

#endif
