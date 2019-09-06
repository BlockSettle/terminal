#include "ChatProtocol/Message.h"

using namespace Chat;

Message::Message(const std::string& partyId, const std::string& messageId, const QDateTime& timestamp,
   const PartyMessageState& partyMessageState, const std::string& messageText, const std::string& sender_hash)
   : partyId_(partyId), messageId_(messageId), timestamp_(timestamp), partyMessageState_(partyMessageState),
   messageText_(messageText), senderHash_(sender_hash)
{
}

Message::Message(const Message& m2) : partyId_(m2.partyId()), messageId_(m2.messageId()), timestamp_(m2.timestamp()),
partyMessageState_(m2.partyMessageState()), messageText_(m2.messageText_), senderHash_(m2.senderHash())
{
}

Message& Message::operator=(const Message& rhs)
{
   if (&rhs != this)
   {
      setPartyId(rhs.partyId());
      setMessageId(rhs.messageId());
      setTimestamp(rhs.timestamp());
      setPartyMessageState(rhs.partyMessageState());
      setMessageText(rhs.messageText());
      setSenderHash(rhs.senderHash());
   }

   return *this;
}

