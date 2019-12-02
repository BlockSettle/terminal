/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef THREAD_NAME_H
#define THREAD_NAME_H

#include <string>

namespace bs {

   // Sets name that is visible in crash dumps and in debugger
   // Implemented for Linux and macOS only for now
   // max length for name is 16 symbols including trailing zero (will be trimmed otherwise)
   void setCurrentThreadName(std::string name);

}

#endif
