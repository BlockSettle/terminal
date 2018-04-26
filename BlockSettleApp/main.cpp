#include <QApplication>
#include <QBitmap>
#include <QCoreApplication>
#include <QDirIterator>
#include <QFile>
#include <QFontDatabase>
#include <QLockFile>
#include <QStandardPaths>
#include <QThread>
#include <QtPlugin>

#include <memory>

#include <spdlog/spdlog.h>

#include "ApplicationSettings.h"
#include "BSTerminalSplashScreen.h"
#include "BSTerminalMainWindow.h"
#include "MessageBoxCritical.h"
#include "MessageBoxInfo.h"


#if defined (Q_OS_WIN)
Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin)
#elif defined (Q_OS_MAC)
Q_IMPORT_PLUGIN(QCocoaIntegrationPlugin)
#elif defined (Q_OS_LINUX)
Q_IMPORT_PLUGIN(QXcbIntegrationPlugin)
#endif

Q_IMPORT_PLUGIN(QICOPlugin)

Q_DECLARE_METATYPE(BinaryDataVector)
Q_DECLARE_METATYPE(BinaryData)

#include <QEvent>
#include <QApplicationStateChangeEvent>

class MacOsApp : public QApplication
{
   Q_OBJECT
public:
   MacOsApp(int &argc, char **argv) : QApplication(argc, argv) {}
   ~MacOsApp() override = default;

signals:
   void reactivateTerminal();
protected:
   bool event(QEvent* ev) override
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

private:
   bool activationRequired_ = false;
};

static int GuiApp(int argc, char** argv)
{
   Q_INIT_RESOURCE(armory);

#if defined (Q_OS_MAC)
   MacOsApp app(argc, argv);
#else
   QApplication app(argc, argv);
#endif

   app.setQuitOnLastWindowClosed(false);
   app.setAttribute(Qt::AA_DontShowIconsInMenus);

   QFileInfo localStyleSheetFile(QLatin1String("stylesheet.css"));

   QFile stylesheetFile(localStyleSheetFile.exists()
                        ? localStyleSheetFile.fileName()
                        : QLatin1String(":/STYLESHEET"));

   if (stylesheetFile.open(QFile::ReadOnly)) {
      app.setStyleSheet(QString::fromLatin1(stylesheetFile.readAll()));
      QPalette p = QApplication::palette();
      p.setColor(QPalette::Disabled, QPalette::Light, QColor(10,22,25));
      QApplication::setPalette(p);
   }

   QDirIterator it(QLatin1String(":/resources/Raleway/"));
   while (it.hasNext()) {
      QFontDatabase::addApplicationFont(it.next());
   }

   QString location = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
   QLockFile lockFile(location + QLatin1String("/blocksettle.lock"));
   lockFile.setStaleLockTime(0);

   if (!lockFile.tryLock()) {
      MessageBoxInfo box(app.tr("BlockSettle Terminal")
         , app.tr("BlockSettle Terminal is already running")
         , app.tr("Another instance of BlockSettle Terminal is running. It may be running in the background, look for the BlockSettle icon in the system tray"));
      return box.exec();
   }

   qRegisterMetaType<QVector<int> >();
   qRegisterMetaType<std::string>();
   qRegisterMetaType<BinaryDataVector>();
   qRegisterMetaType<BinaryData>();

   // load settings
   auto settings = std::make_shared<ApplicationSettings>();
   if (!settings->LoadApplicationSettings(app.arguments())) {
      MessageBoxCritical errorMessage(app.tr("Error")
         , app.tr("Failed to parse command line arguments")
         , settings->ErrorText());
      errorMessage.exec();
      return 1;
   }

   QString logoIcon;
   if (settings->get<NetworkType>(ApplicationSettings::netType) == NetworkType::MainNet) {
      logoIcon = QLatin1String(":/SPLASH_LOGO");
   }
   else {
      logoIcon = QLatin1String(":/SPLASH_LOGO_TESTNET");
   }

   QPixmap splashLogo(logoIcon);
   BSTerminalSplashScreen splashScreen(splashLogo.scaledToWidth(390, Qt::SmoothTransformation));

   splashScreen.show();
   app.processEvents();

   try {
      BSTerminalMainWindow mainWindow(settings, splashScreen);

#if defined (Q_OS_MAC)
      QObject::connect(&app, &MacOsApp::reactivateTerminal, &mainWindow, &BSTerminalMainWindow::onReactivate);
#endif

      if (settings->get<bool>(ApplicationSettings::launchToTray)) {
         splashScreen.close();
      }
      else {
         mainWindow.show();
         splashScreen.finish(&mainWindow);
      }
      return app.exec();
   }
   catch (const std::exception &e) {
      std::cerr << "Failed to start BlockSettle Terminal: " << e.what() << std::endl;
      MessageBoxCritical(app.tr("BlockSettle Terminal"), QLatin1String(e.what())).exec();
      return 1;
   }
   return 0;
}

int main(int argc, char** argv)
{
   return GuiApp(argc, argv);
}

#include "main.moc"
