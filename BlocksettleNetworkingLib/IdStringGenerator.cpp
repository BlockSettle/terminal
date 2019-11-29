/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "IdStringGenerator.h"
#include "EncryptionUtils.h"


IdStringGenerator::IdStringGenerator() : id_(1)
{}

std::string IdStringGenerator::getNextId()
{
   std::stringstream ss;
   ss << std::hex << id_++;
   return ss.str();
}

std::string IdStringGenerator::getUniqueSeed() const
{
   return userName_ + "/" + CryptoPRNG::generateRandom(4).toHexStr();
}
