#include "ChatSessionKeyData.h"

namespace Chat {

   std::string ChatSessionKeyData::receiverId() const
   {
      return _receiverId;
   }

   void ChatSessionKeyData::setReceiverId(const std::string receiverId)
   {
      _receiverId = receiverId;
   }

   SecureBinaryData ChatSessionKeyData::localPrivateKey() const
   {
      return _localPrivateKey;
   }

   void ChatSessionKeyData::setLocalPrivateKey(const SecureBinaryData localPrivateKey)
   {
      _localPrivateKey = localPrivateKey;
   }

   BinaryData ChatSessionKeyData::localPublicKey() const
   {
      return _localPublicKey;
   }

   void ChatSessionKeyData::setLocalPublicKey(const BinaryData localPublicKey)
   {
      _localPublicKey = localPublicKey;
   }

   BinaryData ChatSessionKeyData::remotePublicKey() const
   {
      return _remotePublicKey;
   }

   void ChatSessionKeyData::setRemotePublicKey(const BinaryData remotePublicKey)
   {
      _remotePublicKey = remotePublicKey;
   }

}
