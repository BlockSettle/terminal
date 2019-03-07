#ifndef __CHAT_PROTOCOL_H__
#define __CHAT_PROTOCOL_H__


#include <map>
#include <vector>

#include <QString>
#include <QDateTime>

#include "SecureBinaryData.h"
#include "autheid_utils.h"

#include "DataObjects.h"
#include "RequestObjects.h"
#include "ResponseObjects.h"

   


   class MessageData
   {
   public:
      enum class State {
         Undefined = 0,
         Invalid = 1,
         Encrypted = 2,
         Acknowledged = 4,
         Read = 8,
         Sent = 16
      };

      MessageData(const QString &sender, const QString &receiver
         , const QString &id, const QDateTime &dateTime
         , const QString& messageData, int state = (int)State::Undefined);
      QString getSenderId() const { return senderId_; }
      QString getReceiverId() const { return receiverId_; }
      QString getId() const { return id_; }
      QDateTime getDateTime() const { return dateTime_; }
      QString getMessageData() const { return messageData_; }
      int getState() const { return state_; }
      QJsonObject toJson() const;
      std::string toJsonString() const;
      static std::shared_ptr<MessageData> fromJSON(const std::string& jsonData);

      void setFlag(const State);
      void unsetFlag(const State);
      bool decrypt(const autheid::PrivateKey& privKey);
      bool encrypt(const autheid::PublicKey& pubKey);
      
      //Set ID for message, returns old ID that was replaced
      QString setId(const QString& id);

   private:
      QString id_;
      QString senderId_;
      QString receiverId_;
      QDateTime dateTime_;
      QString messageData_;
      int state_;
   };


   
   
   
   
   

   

   

   

   
   
   
   
   

   

   
 
   
  
   

   

   
   
   

   
   
   
   
   
   
   
   
   
   

   

   

} // namespace Chat


#endif // __CHAT_PROTOCOL_H__
