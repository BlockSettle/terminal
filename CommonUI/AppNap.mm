/*

***********************************************************************************
* Copyright (C) 2019 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "AppNap.h"

#import <Foundation/Foundation.h>

void bs::disableAppNap()
{
   // Must set some reason here
   id activity = [[NSProcessInfo processInfo]beginActivityWithOptions:NSActivityBackground
      reason:@"App_Nap_Not_Supported"];
   [activity retain];
}
