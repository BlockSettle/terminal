#ifndef ChatUser_h__
#define ChatUser_h__

#include <string>
#include <memory>

#include <QObject>

#include <disable_warnings.h>
#include "BinaryData.h"
#include <enable_warnings.h>

namespace Chat
{
   class ChatUser;

   using ChatUserPtr = std::shared_ptr<ChatUser>;

   class ChatUser : public QObject
   {
      Q_OBJECT
   public:
      ChatUser(QObject *parent = nullptr);
      std::string displayName() const;
      void setDisplayName(const std::string& displayName);

      BinaryData publicKey() const { return publicKey_; }
      void setPublicKey(BinaryData val) { publicKey_ = val; }

   signals:
      void displayNameChanged(const std::string& displayName);

   private:
      std::string displayName_;
      BinaryData publicKey_;
   };

}

Q_DECLARE_METATYPE(Chat::ChatUserPtr)
#endif // ChatUser_h__
