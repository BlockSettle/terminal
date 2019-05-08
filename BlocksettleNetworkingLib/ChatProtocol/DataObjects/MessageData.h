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
#include <SecureBinaryData.h>

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
      QString getSenderId() const { return senderId_; }
      QString getReceiverId() const { return receiverId_; }
      QString getId() const { return id_; }
      QDateTime getDateTime() const { return dateTime_; }
      QString getMessageData() const { return messageData_; }
      int getState() const { return state_; }
      QJsonObject toJson() const;
      static std::shared_ptr<MessageData> fromJSON(const std::string& jsonData);
      void setNonce(const Botan::SecureVector<uint8_t> &);
      Botan::SecureVector<uint8_t> getNonce() const;
      size_t getDefaultNonceSize() const;

      void setFlag(const State);
      void unsetFlag(const State);
      bool testFlag(const State stateFlag);
      void updateState(const int newState);
      bool decrypt(const autheid::PrivateKey& privKey);
      bool encrypt(const autheid::PublicKey& pubKey);
      bool encryptAead(const BinaryData& receiverPubKey, const SecureBinaryData& ownPrivKey, const Botan::SecureVector<uint8_t> &nonce, const std::shared_ptr<spdlog::logger>& logger);
      bool decryptAead(const BinaryData& senderPubKey, const SecureBinaryData& privKey, const std::shared_ptr<spdlog::logger>& logger);
      
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
