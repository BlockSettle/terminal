#pragma once

#include "DataObject.h"
#include <memory>

#include <QString>
#include <QDateTime>
#include <QJsonObject>

#include <disable_warnings.h>
#include <botan/secmem.h>
#include <spdlog/spdlog.h>
#include <enable_warnings.h>

namespace Chat {

   class MessageData : public DataObject
   {
   public:
      enum class State {
         Undefined = 0,
         Invalid = 1,
         Acknowledged = 2,
         Read = 4,
         Sent = 8
      };

      enum class EncryptionType {
         Unencrypted = 0,
         IES = 1,
         AEAD = 2
      };

      MessageData(const QString &sender, const QString &receiver,
         const QString &id, const QDateTime &dateTime,
         const QString& messageData,
         int state = (int)State::Undefined);

      QString senderId() const { return senderId_; }
      QString receiverId() const { return receiverId_; }
      QString id() const { return id_; }
      QDateTime dateTime() const { return dateTime_; }
      QString messageData() const { return messageData_; }
      void setMessageData(const QString& messageData);
      int state() const { return state_; }
      std::string jsonAssociatedData() const;

      QJsonObject toJson() const override;
      static std::shared_ptr<MessageData> fromJSON(const std::string& jsonData);

      void setNonce(const Botan::SecureVector<uint8_t> &);
      Botan::SecureVector<uint8_t> nonce() const;
      size_t defaultNonceSize() const;

      void setFlag(const State);
      void unsetFlag(const State);
      bool testFlag(const State stateFlag);
      void updateState(const int newState);

      //Set ID for message, returns old ID that was replaced
      QString setId(const QString& id);

      MessageData::EncryptionType encryptionType() const;
      void setEncryptionType(const MessageData::EncryptionType &type);

   private:
      QString id_;
      QString senderId_;
      QString receiverId_;
      QDateTime dateTime_;
      QString messageData_;
      int state_;
      Botan::SecureVector<uint8_t> nonce_;
      EncryptionType encryptionType_;
   };
}
