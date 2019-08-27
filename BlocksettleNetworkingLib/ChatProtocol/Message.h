#ifndef Message_h__
#define Message_h__

#include <QMetaType>

#include <memory>
#include <string>

namespace Chat
{
   enum PartyMessageState : int;

   class Message
   {
   public:
      Message(const std::string& partyId, const std::string& messageId, const long long timestamp,
         const PartyMessageState partyMessageState, const std::string& messageText, const std::string& sender_hash);

      Message(const Message& m2);

      Message& operator=(const Message& rhs);

      const std::string& partyId() const { return partyId_; }
      void setPartyId(std::string val) { partyId_ = val; }

      const std::string& messageId() const { return messageId_; }
      void setMessageId(std::string val) { messageId_ = val; }

      const long long& timestamp() const { return timestamp_; }
      void setTimestamp(long long val) { timestamp_ = val; }

      const PartyMessageState& partyMessageState() const { return partyMessageState_; }
      void setPartyMessageState(PartyMessageState val) { partyMessageState_ = val; }

      const std::string& messageText() const { return messageText_; }
      void setMessageText(std::string val) { messageText_ = val; }

      const std::string& senderHash() const { return senderHash_; }
      void setSenderHash(std::string val) { senderHash_ = val; }

   private:
      std::string partyId_;
      std::string messageId_;
      long long timestamp_;
      PartyMessageState partyMessageState_;
      std::string messageText_;
      std::string senderHash_;
   };

   using MessagePtr = std::shared_ptr<Message>;
   using MessagePtrList = std::vector<MessagePtr>;
}

Q_DECLARE_METATYPE(Chat::MessagePtr)
Q_DECLARE_METATYPE(Chat::MessagePtrList);

#endif // Message_h__
