#include "ChatProtocol/Message.h"

namespace Chat
{

   Message::Message(const std::string& partyId, const std::string& messageId, const long long timestamp,
      const int partyMessageState, const std::string& messageText, const std::string& sender)
      : partyId_(partyId), messageId_(messageId), timestamp_(timestamp), partyMessageState_(partyMessageState),
      messageText_(messageText), sender_(sender)
   {

   }

}