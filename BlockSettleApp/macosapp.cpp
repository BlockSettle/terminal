#include <QEvent>
#include <QApplicationStateChangeEvent>
#include "macosapp.h"

MacOsApp::MacOsApp(int &argc, char **argv) : QApplication(argc, argv) 
{
 // EMPTY CONSTRUCTOR
}
bool MacOsApp::event(QEvent* ev)
{
      if (ev->type() ==  QEvent::ApplicationStateChange) {
         auto appStateEvent = static_cast<QApplicationStateChangeEvent*>(ev);

         if (appStateEvent->applicationState() == Qt::ApplicationActive) {
            if (activationRequired_) {
               emit reactivateTerminal();
            } else {
               activationRequired_ = true;
            }
         } else {
            activationRequired_ = false;
         }
      }

      return QApplication::event(ev);
   }
