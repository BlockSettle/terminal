#include "ChatProtocol/ChatUser.h"

namespace Chat
{

   ChatUser::ChatUser(QObject *parent) : QObject(parent)
   {
   }

   std::string ChatUser::displayName() const
   {
      return displayName_;
   }

   void ChatUser::setDisplayName(const std::string& displayName)
   {
      displayName_ = displayName;

      emit displayNameChanged(displayName_);
   }

}
