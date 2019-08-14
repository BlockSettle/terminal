#ifndef ChatUser_h__
#define ChatUser_h__

#include <string>
#include <memory>

#include <QObject>

#include <disable_warnings.h>
#include "BinaryData.h"
#include "SecureBinaryData.h"
#include <enable_warnings.h>

namespace Chat
{

   class ChatUser : public QObject
   {
      Q_OBJECT
   public:
      ChatUser(QObject *parent = nullptr);
      std::string displayName() const;
      void setDisplayName(const std::string& displayName);

      BinaryData publicKey() const { return publicKey_; }
      void setPublicKey(BinaryData val) { publicKey_ = val; }

      SecureBinaryData privateKey() const { return privateKey_; }
      void setPrivateKey(SecureBinaryData val) { privateKey_ = val; }

   signals:
      void displayNameChanged(const std::string& displayName);

   private:
      std::string displayName_;
      BinaryData publicKey_;
      SecureBinaryData privateKey_;
   };

   using ChatUserPtr = std::shared_ptr<ChatUser>;

}

Q_DECLARE_METATYPE(Chat::ChatUserPtr)

#endif // ChatUser_h__
