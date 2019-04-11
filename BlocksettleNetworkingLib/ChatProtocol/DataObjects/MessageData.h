#pragma once

#include "DataObject.h"
#include <memory>

#include <QString>
#include <QDateTime>
#include <QJsonObject>

#include <disable_warnings.h>
#include "autheid_utils.h"
#include <spdlog/spdlog.h>
#include <enable_warnings.h>

namespace Chat {

   class MessageData : public DataObject
   {
   public:
      enum class State {
         Undefined = 0,
         Invalid = 1,
         Encrypted = 2,
         Acknowledged = 4,
         Read = 8,
         Sent = 16,
         Encrypted_AEAD = 32
      };
   
      MessageData(const QString &sender, const QString &receiver,
         const QString &id, const QDateTime &dateTime,
         const QString& messageData, 
         int state = (int)State::Undefined);
      QString getSenderId() const { return senderId_; }
      QString getReceiverId() const { return receiverId_; }
      QString getId() const { return id_; }
      QDateTime getDateTime() const { return dateTime_; }
      QString getMessageData() const { return messageData_; }
      int getState() const { return state_; }
      QJsonObject toJson() const;
      static std::shared_ptr<MessageData> fromJSON(const std::string& jsonData);
      void setNonce(const autheid::SecureBytes &);
      QString getNonce() const;
      size_t getDefaultNonceSize() const;

      void setFlag(const State);
      void unsetFlag(const State);
      void updateState(const int newState);
      bool decrypt(const autheid::PrivateKey& privKey);
      bool encrypt(const autheid::PublicKey& pubKey);
      bool encrypt_aead(const autheid::PublicKey& receiverPubKey, const autheid::PrivateKey& ownPrivKey, const autheid::SecureBytes &nonce, const std::shared_ptr<spdlog::logger>& logger);
      bool decrypt_aead(const autheid::PublicKey& senderPubKey, const autheid::PrivateKey& privKey, const std::shared_ptr<spdlog::logger>& logger);
      
      //Set ID for message, returns old ID that was replaced
      QString setId(const QString& id);
   
   private:
      QString id_;
      QString senderId_;
      QString receiverId_;
      QDateTime dateTime_;
      QString messageData_;
      int state_;
      autheid::SecureBytes nonce_;
   };
}
