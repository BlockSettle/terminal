#ifndef ChatUser_h__
#define ChatUser_h__

#include <string>
#include <memory>

#include <QObject>

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

   signals:
      void displayNameChanged(const std::string& displayName);

   private:
      std::string displayName_;
   };

   Q_DECLARE_METATYPE(ChatUserPtr)

}

#endif // ChatUser_h__