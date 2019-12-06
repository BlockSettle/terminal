#include "AppNap.h"

#import <Foundation/Foundation.h>

void bs::disableAppNap()
{
   // Must set some reason here
   id activity = [[NSProcessInfo processInfo]beginActivityWithOptions:NSActivityBackground
      reason:@"App_Nap_Not_Supported"];
   [activity retain];
}
