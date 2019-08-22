#ifndef Message_h__
#define Message_h__

#include <QMetaType>

#include <memory>
#include <string>

namespace Chat
{

   class Message
   {
   public:
      Message(const std::string& partyId, const std::string& messageId, const long long timestamp,
         const int partyMessageState, const std::string& messageText, const std::string& sender);

      std::string partyId() const { return partyId_; }
      void setPartyId(std::string val) { partyId_ = val; }

      std::string messageId() const { return messageId_; }
      void setMessageId(std::string val) { messageId_ = val; }

      long long timestamp() const { return timestamp_; }
      void setTimestamp(long long val) { timestamp_ = val; }

      int partyMessageState() const { return partyMessageState_; }
      void setPartyMessageState(int val) { partyMessageState_ = val; }

      std::string messageText() const { return messageText_; }
      void setMessageText(std::string val) { messageText_ = val; }

      std::string sender() const { return sender_; }
      void setSender(std::string val) { sender_ = val; }

   private:
      std::string partyId_;
      std::string messageId_;
      long long timestamp_;
      int partyMessageState_;
      std::string messageText_;
      std::string sender_;
   };

   using MessagePtr = std::shared_ptr<Message>;
}

Q_DECLARE_METATYPE(Chat::MessagePtr)

#endif // Message_h__
