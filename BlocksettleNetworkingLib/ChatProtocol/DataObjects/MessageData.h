#ifndef MessageData_h__
#define MessageData_h__

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

   class OTCRequestData;

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

      enum RawMessageDataType
      {
         Undefined,
         TextMessage,
         OTCReqeust,
         OTCResponse,
         OTCUpdate,
         OTCCloseTrading
      };

      enum class EncryptionType {
         Unencrypted = 0,
         IES = 1,
         AEAD = 2
      };

      enum class MessageDirection
      {
         NotSet,
         Sent,
         Received
      };

      MessageData(const QJsonObject& jsonData);
      // create unencrypted message from current
      MessageData(const MessageData& source, const QJsonObject& jsonData);

      // placeholder for derived classes
      MessageData(const QString &sender, const QString &receiver
                  , const QString &id, const QDateTime &dateTime
                  , const QString&  messagePayload
                  , RawMessageDataType rawType
                  , int state = (int)State::Undefined);

      MessageData(const MessageData& source, RawMessageDataType rawType);

      // create regular unencrypted message pbject
      MessageData(const QString &sender, const QString &receiver
                  , const QString &id, const QDateTime &dateTime
                  , const QString& messageText
                  , int state = (int)State::Undefined);

      // create encrypted message from current
      // raw type allways TextMessage
      MessageData(const MessageData& source
                  , const MessageData::EncryptionType &type
                  , const QString& encryptedPayload);

   private:
      MessageData(const MessageData& source);
   public:

      std::shared_ptr<MessageData> CreateEncryptedMessage(const MessageData::EncryptionType &type, const QString& messagePayload);
      std::shared_ptr<MessageData> CreateDecryptedMessage(const QString& messagePayload);

      QString senderId() const { return senderId_; }
      QString receiverId() const { return receiverId_; }
      QString id() const { return id_; }
      QDateTime dateTime() const { return dateTime_; }

      RawMessageDataType messageDataType() const;

      MessageDirection messageDirectoin() const;
      void setMessageDirection(MessageDirection direction);

      // text for display on UI
      virtual QString displayText() const;
      // used for serialization only
      QString messagePayload() const;

      virtual void messageDirectionUpdate();

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

      static QString directionToText(MessageDirection direction);

      bool loadedFromHistory() const;
      void setLoadedFromHistory();

   protected:
      void updatePayload(const QString& payload);

   private:
      QString serializePayload();

   private:
      QString        id_;
      QString        senderId_;
      QString        receiverId_;
      QDateTime      dateTime_;
      int            state_;
      Botan::SecureVector<uint8_t> nonce_;
      MessageDirection     direction_ = MessageDirection::NotSet;
      EncryptionType       encryptionType_;
      QString              displayText_;
      QString              messagePayload_;

      RawMessageDataType   rawType_ = RawMessageDataType::Undefined;
      bool loadedFromHistory_ = false;
   };
}

#endif // MessageData_h__
