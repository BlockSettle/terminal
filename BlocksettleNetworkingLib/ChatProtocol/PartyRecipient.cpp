#include "PartyRecipient.h"

using namespace Chat;

Chat::PartyRecipient::PartyRecipient(const std::string& userHash, const BinaryData& publicKey, const QDateTime& publicKeyTime)
   : userHash_(userHash), publicKey_(publicKey), publicKeyTime_(publicKeyTime)
{
}
