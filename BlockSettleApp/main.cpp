#include <QApplication>
#include <QBitmap>
#include <QCoreApplication>
#include <QDateTime>
#include <QDirIterator>
#include <QFile>
#include <QFontDatabase>
#include <QLockFile>
#include <QStandardPaths>
#include <QThread>
#include <QtPlugin>

#include <memory>

#include "ApplicationSettings.h"
#include "BSTerminalSplashScreen.h"
#include "BSTerminalMainWindow.h"
#include "EncryptionUtils.h"
#include "BSMessageBox.h"

#include "btc/ecc.h"

#ifdef USE_QWindowsIntegrationPlugin
Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin)
Q_IMPORT_PLUGIN(QWindowsPrinterSupportPlugin)
#endif // USE_QWindowsIntegrationPlugin

#ifdef USE_QCocoaIntegrationPlugin
Q_IMPORT_PLUGIN(QCocoaIntegrationPlugin)
Q_IMPORT_PLUGIN(QCocoaPrinterSupportPlugin)
#endif // USE_QCocoaIntegrationPlugin

#ifdef USE_QXcbIntegrationPlugin
Q_IMPORT_PLUGIN(QXcbIntegrationPlugin)
Q_IMPORT_PLUGIN(QCupsPrinterSupportPlugin)
#endif // USE_QXcbIntegrationPlugin

#ifdef STATIC_BUILD
Q_IMPORT_PLUGIN(QSQLiteDriverPlugin)
Q_IMPORT_PLUGIN(QICOPlugin)
#endif // STATIC_BUILD

Q_DECLARE_METATYPE(std::string)
Q_DECLARE_METATYPE(BinaryData)
Q_DECLARE_METATYPE(SecureBinaryData)
Q_DECLARE_METATYPE(std::vector<BinaryData>)
Q_DECLARE_METATYPE(UTXO)
Q_DECLARE_METATYPE(std::vector<UTXO>)
Q_DECLARE_METATYPE(AsyncClient::LedgerDelegate)
Q_DECLARE_METATYPE(std::shared_ptr<std::promise<bool>>)
Q_DECLARE_METATYPE(ArmorySettings)

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

static void checkStyleSheet(QApplication &app)
{
   QLatin1String styleSheetFileName = QLatin1String("stylesheet.css");

   QFileInfo info = QFileInfo(QLatin1String(styleSheetFileName));

   static QDateTime lastTimestamp = info.lastModified();

   if (lastTimestamp == info.lastModified()) {
      return;
   }

   lastTimestamp = info.lastModified();

   QFile stylesheetFile(styleSheetFileName);

   bool result = stylesheetFile.open(QFile::ReadOnly);
   assert(result);

   app.setStyleSheet(QString::fromLatin1(stylesheetFile.readAll()));
}

static int GuiApp(int &argc, char** argv)
{
   Q_INIT_RESOURCE(armory);

   QApplication::setAttribute(Qt::AA_DontShowIconsInMenus);
   QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);

#if defined (Q_OS_MAC)
   MacOsApp app(argc, argv);
#else
   QApplication app(argc, argv);
#endif

   QApplication::setQuitOnLastWindowClosed(false);

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

#ifndef NDEBUG
   // Start monitoring to update stylesheet live when file is changed on the disk
   QTimer timer;
   QObject::connect(&timer, &QTimer::timeout, &app, [&app] {
      checkStyleSheet(app);
   });
   timer.start(100);
#endif

   QDirIterator it(QLatin1String(":/resources/Raleway/"));
   while (it.hasNext()) {
      QFontDatabase::addApplicationFont(it.next());
   }

   QString location = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
#ifndef NDEBUG
   QString userName = QDir::home().dirName();
   QString lockFilePath = location + QLatin1String("/blocksettle-") + userName + QLatin1String(".lock");
#else
   QString lockFilePath = location + QLatin1String("/blocksettle.lock");
#endif
   QLockFile lockFile(lockFilePath);
   lockFile.setStaleLockTime(0);

   if (!lockFile.tryLock()) {
      BSMessageBox box(BSMessageBox::info, app.tr("BlockSettle Terminal")
         , app.tr("BlockSettle Terminal is already running")
         , app.tr("Stop the other BlockSettle Terminal instance. If no other " \
         "instance is running, delete the lockfile (%1).").arg(lockFilePath));
      return box.exec();
   }

   qRegisterMetaType<QVector<int>>();
   qRegisterMetaType<std::string>();
   qRegisterMetaType<BinaryData>();
   qRegisterMetaType<SecureBinaryData>();
   qRegisterMetaType<std::vector<BinaryData>>();
   qRegisterMetaType<UTXO>();
   qRegisterMetaType<std::vector<UTXO>>();
   qRegisterMetaType<AsyncClient::LedgerDelegate>();
   qRegisterMetaType<std::shared_ptr<std::promise<bool>>>();
   qRegisterMetaType<ArmorySettings>();

   // load settings
   auto settings = std::make_shared<ApplicationSettings>();
   if (!settings->LoadApplicationSettings(app.arguments())) {
      BSMessageBox errorMessage(BSMessageBox::critical, app.tr("Error")
         , app.tr("Failed to parse command line arguments")
         , settings->ErrorText());
      errorMessage.exec();
      return EXIT_FAILURE;
   }

   QString logoIcon;
   logoIcon = QLatin1String(":/SPLASH_LOGO");

   QPixmap splashLogo(logoIcon);
   const int splashScreenWidth = 400;
   BSTerminalSplashScreen splashScreen(splashLogo.scaledToWidth(splashScreenWidth, Qt::SmoothTransformation));

   splashScreen.show();
   app.processEvents();

#ifndef _DEBUG
   try {
#endif
      BSTerminalMainWindow mainWindow(settings, splashScreen);

#if defined (Q_OS_MAC)
      QObject::connect(&app, &MacOsApp::reactivateTerminal, &mainWindow, &BSTerminalMainWindow::onReactivate);
#endif

      if (!settings->get<bool>(ApplicationSettings::launchToTray)) {
         mainWindow.show();
      }

      mainWindow.postSplashscreenActions();

      return app.exec();
#ifndef _DEBUG
   }
   catch (const std::exception &e) {
      std::cerr << "Failed to start BlockSettle Terminal: " << e.what() << std::endl;
      BSMessageBox(BSMessageBox::critical, app.tr("BlockSettle Terminal"), QLatin1String(e.what())).exec();
      return 1;
   }
   return 0;
#endif // _DEBUG
}

int main(int argc, char** argv)
{
   srand(std::time(nullptr));

   // Initialize libbtc, BIP 150, and BIP 151. 150 uses the proprietary "public"
   // Armory setting designed to allow the ArmoryDB server to not have to verify
   // clients. Prevents us from having to import tons of keys into the server.
   btc_ecc_start();
   startupBIP151CTX();
   startupBIP150CTX(4, true);

   return GuiApp(argc, argv);
}

#include "main.moc"
