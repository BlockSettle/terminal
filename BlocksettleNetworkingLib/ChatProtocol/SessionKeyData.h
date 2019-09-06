#ifndef SESSIONKEYDATA_H
#define SESSIONKEYDATA_H

#include <memory>
#include <string>

#include <QMetaType>

#include <disable_warnings.h>
#include "BinaryData.h"
#include "SecureBinaryData.h"
#include <botan/secmem.h>
#include <enable_warnings.h>

namespace Chat
{

   class SessionKeyData
   {
   public:
      SessionKeyData(const std::string& userName);
      SessionKeyData(const std::string& userName, const BinaryData& localSessionPublicKey, const SecureBinaryData& localSessionPrivateKey);

      std::string userName() const { return userName_; }
      void setUserName(std::string val) { userName_ = val; }

      BinaryData remoteSessionPublicKey() const { return remoteSessionPublicKey_; }
      void setSessionRemotePublicKey(BinaryData val)
      {
         remoteSessionPublicKey_ = val;
         initialized_ = true;
      }

      bool isInitialized() const { return initialized_; }
      void setInitialized(const bool val) { initialized_ = val; }

      BinaryData localSessionPublicKey() const { return localSessionPublicKey_; }
      void setLocalSessionPublicKey(BinaryData val) { localSessionPublicKey_ = val; }

      SecureBinaryData localSessionPrivateKey() const { return localSessionPrivateKey_; }
      void setLocalSessionPrivateKey(SecureBinaryData val) { localSessionPrivateKey_ = val; }

      BinaryData nonce();

   private:
      std::string userName_;
      BinaryData localSessionPublicKey_;
      BinaryData remoteSessionPublicKey_;
      SecureBinaryData localSessionPrivateKey_;
      bool initialized_{ false };
      Botan::SecureVector<uint8_t> nonce_;
   };

   using SessionKeyDataPtr = std::shared_ptr<SessionKeyData>;

}

Q_DECLARE_METATYPE(Chat::SessionKeyDataPtr)

#endif // SESSIONKEYDATA_H
