/*

***********************************************************************************
* Copyright (C) 2016 - , BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef TREZORSTRUCTURE_H
#define TREZORSTRUCTURE_H

#include "hwcommonstructure.h"

enum class State {
   None = 0,
   Init,
   Enumerated,
   Acquired,
   Released
};

struct MessageData
{
   int msg_type_ = -1;
   int length_ = -1;
   std::string message_;
};

#endif // TREZORSTRUCTURE_H
