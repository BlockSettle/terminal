/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef APP_NAP_H
#define APP_NAP_H

namespace bs {

#ifdef __APPLE__
   // Disable App Nap (macOS power-saving feature)
   void disableAppNap();
#else
   // Noop
   void disableAppNap() {}
#endif

}

#endif
