#pragma once
#include "DataObject.h"

namespace Chat {
   
   class ChatRoomData : public DataObject
   {
   public:
      class UserRecord {
      public:
         UserRecord(const QString& userId, bool isAdmin)
            : userId_(userId)
            , isAdmin_(isAdmin) {}
         
         QString getUserId() { return userId_; }
         bool isAdmin() { return isAdmin_; }
      private:
         QString userId_;
         bool isAdmin_;
      };
      
      ChatRoomData(const QString& roomId, 
               const QString& ownerId,
               const QString& roomTitle = QLatin1String("noname room"),
               const QString& roomKey = QLatin1String(""),
               bool isPrivate = false,
               bool sendUserUpdates = true,
               bool displayUserList = true);
      
      QString getId();
      QString getOwnerId();
      QString getTitle();
      QString getRoomKey();
      bool isPrivate();
      bool sendUserUpdates();
      bool displayUserList();

      bool haveNewMessage() const;
      void setHaveNewMessage(bool haveNewMessage);
      
   private:
      QString id_;
      QString ownerId_;
      QString title_;
      QString roomKey_;
      bool isPrivate_;
      bool sendUserUpdates_;
      bool displayUserList_;
      bool haveNewMessage_;
      //QList<UserRecord> userList_;
      
      
      // DataObject interface
   public:
      QJsonObject toJson() const;
      static std::shared_ptr<ChatRoomData> fromJSON(const std::string& jsonData);
   };

   using ChatRoomDataPtr = std::shared_ptr<ChatRoomData>;
   using ChatRoomDataListPtr = QList<ChatRoomDataPtr>;
}
