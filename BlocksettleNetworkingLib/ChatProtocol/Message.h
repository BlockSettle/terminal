/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef MESSAGE_H
#define MESSAGE_H

#include <QMetaType>
#include <QDateTime>

#include <memory>
#include <string>

namespace Chat
{
   enum PartyMessageState : int;

   class Message
   {
   public:
      Message(std::string partyId, std::string messageId, QDateTime timestamp,
         const PartyMessageState& partyMessageState, std::string messageText, std::string sender_hash);

      Message(const Message& m2);

      Message& operator=(const Message& rhs);

      ~Message() = default;
      Message(Message&&) = default;
      Message& operator=(Message&&) = default;

      const std::string& partyId() const { return partyId_; }
      void setPartyId(const std::string& val) { partyId_ = val; }

      const std::string& messageId() const { return messageId_; }
      void setMessageId(const std::string& val) { messageId_ = val; }

      const QDateTime& timestamp() const { return timestamp_; }
      void setTimestamp(const QDateTime& val) { timestamp_ = val; }

      const PartyMessageState& partyMessageState() const { return partyMessageState_; }
      void setPartyMessageState(const PartyMessageState& val) { partyMessageState_ = val; }

      const std::string& messageText() const { return messageText_; }
      void setMessageText(const std::string& val) { messageText_ = val; }

      const std::string& senderHash() const { return senderHash_; }
      void setSenderHash(const std::string& val) { senderHash_ = val; }

      std::string displayName() const { return displayName_; }
      void setDisplayName(const std::string& display_name) { displayName_ = display_name; }

   private:
      std::string partyId_;
      std::string messageId_;
      QDateTime timestamp_;
      PartyMessageState partyMessageState_;
      std::string messageText_;
      std::string senderHash_;
      std::string displayName_;
   };

   using MessagePtr = std::shared_ptr<Message>;
   using MessagePtrList = std::vector<MessagePtr>;
}

Q_DECLARE_METATYPE(Chat::MessagePtr)
Q_DECLARE_METATYPE(Chat::MessagePtrList);

#endif // MESSAGE_H
