#include "PartyRecipient.h"

using namespace Chat;

Chat::PartyRecipient::PartyRecipient(const std::string& userName, const BinaryData& publicKey, const QDateTime& publicKeyTime)
   : userName_(userName), publicKey_(publicKey), publicKeyTime_(publicKeyTime)
{
}
