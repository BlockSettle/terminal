/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef ZMQ_BIP15X_HELPERS_H
#define ZMQ_BIP15X_HELPERS_H

#include <functional>
#include <future>
#include <string>
#include "BinaryData.h"

class AuthorizedPeers;
class ZmqBIP15XDataConnection;
struct btc_pubkey_;
using btc_pubkey = btc_pubkey_;
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
   std::string name_;

   // EC public key (33 bytes if compressed)
   BinaryData pubKey_;
};

using ZmqBIP15XPeers = std::vector<ZmqBIP15XPeer>;

class ZmqBIP15XUtils
{
public:
   static BinaryData convertKey(const btc_pubkey &pubKey);

   // Convert to BinaryData, return empty result if key is not compressed
   static BinaryData convertCompressedKey(const btc_pubkey &pubKey);

   static bool isValidPubKey(const BinaryData &pubKey);

   static bool addAuthPeer(AuthorizedPeers *authPeers, const ZmqBIP15XPeer &peer);

   static void updatePeerKeys(AuthorizedPeers *authPeers, const std::vector<ZmqBIP15XPeer> &peers);
};

using ZmqBipNewKeyCb = std::function<void(const std::string &oldKey, const std::string &newKey
   , const std::string& srvAddrPort, const std::shared_ptr<FutureValue<bool>> &prompt)>;

using ZmqBIP15XDataConnectionPtr = std::shared_ptr<ZmqBIP15XDataConnection>;

#endif
