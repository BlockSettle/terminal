#include "NativeEventFilter.h"
#include <QCoreApplication>

#ifdef Q_OS_WIN
#include "Windows.h"
#include "Windowsx.h"
#endif

bool NativeEventFilter::nativeEventFilter(const QByteArray &eventType, void *message, long *result)
{
#ifdef Q_OS_WIN
   if (eventType != "windows_generic_MSG")
      return false;

   MSG* msg = static_cast<MSG*>(message);

   switch (msg->message) {
   case WM_CLOSE:
   {
      qApp->quit();
      break;
   }

   default:
      break;
   }

   return false;
#else
   return false;
#endif
}
