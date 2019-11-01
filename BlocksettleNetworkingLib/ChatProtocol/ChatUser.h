#ifndef CHATUSER_H
#define CHATUSER_H

#include <string>
#include <memory>

#include <QObject>

#include <disable_warnings.h>
#include "BinaryData.h"
#include "SecureBinaryData.h"
#include <enable_warnings.h>

#include "CommonTypes.h"

namespace Chat
{

   class ChatUser : public QObject
   {
      Q_OBJECT
   public:
      ChatUser(QObject *parent = nullptr);
      std::string userName() const;
      void setUserName(const std::string& userName);

      BinaryData publicKey() const { return publicKey_; }
      void setPublicKey(const BinaryData& val) { publicKey_ = val; }

      SecureBinaryData privateKey() const { return privateKey_; }
      void setPrivateKey(const SecureBinaryData& val) { privateKey_ = val; }

      bs::network::UserType celerUserType() const { return celerUserType_; }
      void setCelerUserType(const bs::network::UserType& val) { celerUserType_ = val; }
   signals:
      void userNameChanged(const std::string& displayName);

   private:
      std::string userName_;
      BinaryData publicKey_;
      SecureBinaryData privateKey_;
      bs::network::UserType  celerUserType_ = bs::network::UserType::Undefined;
   };

   using ChatUserPtr = std::shared_ptr<ChatUser>;
}

#endif // CHATUSER_H
