#include "PartyRecipient.h"

namespace Chat
{

   Chat::PartyRecipient::PartyRecipient(const std::string& userName, const BinaryData& publicKey)
      : userName_(userName), publicKey_(publicKey)
   {
   }

}
