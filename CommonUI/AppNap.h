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
