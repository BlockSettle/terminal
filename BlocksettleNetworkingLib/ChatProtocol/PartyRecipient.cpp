#include "PartyRecipient.h"
#include <utility>

using namespace Chat;

PartyRecipient::PartyRecipient(std::string userHash, BinaryData publicKey, QDateTime publicKeyTime)
   : userHash_(std::move(userHash)), publicKey_(std::move(publicKey)), publicKeyTime_(std::move(publicKeyTime))
{
}
