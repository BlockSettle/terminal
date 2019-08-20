#include "ChatProtocol/SessionKeyData.h"

namespace Chat
{

   SessionKeyData::SessionKeyData(const std::string& userName)
      : userName_(userName)
   {

   }

   SessionKeyData::SessionKeyData(const std::string& userName, const BinaryData& localSessionPublicKey, const SecureBinaryData& localSessionPrivateKey)
      : userName_(userName), localSessionPublicKey_(localSessionPublicKey), localSessionPrivateKey_(localSessionPrivateKey)
   {

   }

}