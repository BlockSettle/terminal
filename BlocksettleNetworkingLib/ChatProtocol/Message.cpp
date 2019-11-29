/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include <utility>
#include "ChatProtocol/Message.h"

using namespace Chat;

Message::Message(std::string partyId, std::string messageId, QDateTime timestamp,
   const PartyMessageState& partyMessageState, std::string messageText, std::string sender_hash)
   : partyId_(std::move(partyId)), messageId_(std::move(messageId)), timestamp_(std::move(timestamp)), partyMessageState_(partyMessageState),
   messageText_(std::move(messageText)), senderHash_(std::move(sender_hash))
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

