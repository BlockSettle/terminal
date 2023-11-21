#ifndef __MACOSAPP_H__
#define __MACOSAPP_H__

#include <QApplication>
#include <QEvent>
#include <QApplicationStateChangeEvent>

class MacOsApp : public QApplication
{
   Q_OBJECT
public:
    using QApplication::QApplication;
   MacOsApp(int &argc, char **argv); // : QApplication(argc, argv) {}
//   ~MacOsApp() override = default;

signals:
   void reactivateTerminal();

protected:
   bool event(QEvent* ev) override;

private:
   bool activationRequired_ = false;
};

#endif
