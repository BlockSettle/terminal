#ifndef CHATUSER_H
#define CHATUSER_H

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
      std::string userName() const;
      void setUserName(const std::string& displayName);

      BinaryData publicKey() const { return publicKey_; }
      void setPublicKey(BinaryData val) { publicKey_ = val; }

      SecureBinaryData privateKey() const { return privateKey_; }
      void setPrivateKey(SecureBinaryData val) { privateKey_ = val; }

   signals:
      void userNameChanged(const std::string& displayName);

   private:
      std::string userName_;
      BinaryData publicKey_;
      SecureBinaryData privateKey_;
   };

   using ChatUserPtr = std::shared_ptr<ChatUser>;
}

#endif // CHATUSER_H
