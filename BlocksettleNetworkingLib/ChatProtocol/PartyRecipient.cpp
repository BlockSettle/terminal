/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "PartyRecipient.h"
#include <utility>

using namespace Chat;

PartyRecipient::PartyRecipient(std::string userHash, BinaryData publicKey, QDateTime publicKeyTime)
   : userHash_(std::move(userHash)), publicKey_(std::move(publicKey)), publicKeyTime_(std::move(publicKeyTime))
{
}
