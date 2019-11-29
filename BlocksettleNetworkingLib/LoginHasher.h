/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef LOGIN_HASHER_H
#define LOGIN_HASHER_H

#include <string>
#include "UserHasher.h"

class LoginHasher
{
public:
   // Should be 32 bytes long
   LoginHasher(const BinaryData &salt);

   // Switch to lower case and apply UserHasher
   std::string hashLogin(const std::string &login) const;
private:
   const UserHasher hasher_;
};

#endif
