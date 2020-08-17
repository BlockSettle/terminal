/*

***********************************************************************************
* Copyright (C) 2018 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "QtGuiAdapter.h"
#include <QApplication>
#include <QDateTime>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QLockFile>
#include <QScreen>
#include <QSettings>
#include <QStandardPaths>
#include <QThread>
#include <QTimer>
#include <spdlog/spdlog.h>
#include "AppNap.h"
#include "BSMessageBox.h"
#include "BSTerminalSplashScreen.h"
#include "MainWindow.h"

#include "terminal.pb.h"

using namespace BlockSettle::Terminal;

Q_DECLARE_METATYPE(bs::error::AuthAddressSubmitResult);
Q_DECLARE_METATYPE(std::string)

#if defined (Q_OS_MAC)
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
      if (ev->type() == QEvent::ApplicationStateChange) {
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
#endif   // Q_OS_MAC


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

static QScreen *getDisplay(QPoint position)
{
   for (auto currentScreen : QGuiApplication::screens()) {
      if (currentScreen->availableGeometry().contains(position, false)) {
         return currentScreen;
      }
   }

   return QGuiApplication::primaryScreen();
}


QtGuiAdapter::QtGuiAdapter(const std::shared_ptr<spdlog::logger> &logger)
   : QObject(nullptr), logger_(logger)
   , userSettings_(std::make_shared<bs::message::UserTerminal>(bs::message::TerminalUsers::Settings))
{}

QtGuiAdapter::~QtGuiAdapter()
{}

void QtGuiAdapter::run(int &argc, char **argv)
{
   logger_->debug("[QtGuiAdapter::run]");

   Q_INIT_RESOURCE(armory);
   Q_INIT_RESOURCE(tradinghelp);
   Q_INIT_RESOURCE(wallethelp);

   QApplication::setAttribute(Qt::AA_DontShowIconsInMenus);
   QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);

#if defined (Q_OS_MAC)
   MacOsApp app(argc, argv);
#else
   QApplication app(argc, argv);
#endif

   QApplication::setQuitOnLastWindowClosed(false);

   const QFileInfo localStyleSheetFile(QLatin1String("stylesheet.css"));
   QFile stylesheetFile(localStyleSheetFile.exists()
      ? localStyleSheetFile.fileName() : QLatin1String(":/STYLESHEET"));

   if (stylesheetFile.open(QFile::ReadOnly)) {
      app.setStyleSheet(QString::fromLatin1(stylesheetFile.readAll()));
      QPalette p = QApplication::palette();
      p.setColor(QPalette::Disabled, QPalette::Light, QColor(10, 22, 25));
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
      box.exec();
      return;
   }

   qRegisterMetaType<bs::error::AuthAddressSubmitResult>();
   qRegisterMetaType<QVector<int>>();
   qRegisterMetaType<std::string>();

   QString logoIcon;
   logoIcon = QLatin1String(":/SPLASH_LOGO");

   QPixmap splashLogo(logoIcon);
   const int splashScreenWidth = 400;
   splashScreen_ = new BSTerminalSplashScreen(splashLogo.scaledToWidth(splashScreenWidth
      , Qt::SmoothTransformation));
   splashScreen_->show();

   mainWindow_ = new bs::gui::qt::MainWindow(logger_, queue_, user_);
   updateSplashProgress();

   requestInitialSettings();
   logger_->debug("[QtGuiAdapter::run] initial setup done");

#if defined (Q_OS_MAC)
   MacOsApp *macApp = (MacOsApp*)(app);
   QObject::connect(macApp, &MacOsApp::reactivateTerminal, mainWindow
      , &bs::gui::qt::MainWindow::onReactivate);
#endif
   bs::disableAppNap();

   if (app.exec() != 0) {
      throw std::runtime_error("application execution failed");
   }
}

bool QtGuiAdapter::process(const bs::message::Envelope &env)
{
   if (std::dynamic_pointer_cast<bs::message::UserTerminal>(env.sender)) {
      switch (env.sender->value<bs::message::TerminalUsers>()) {
      case bs::message::TerminalUsers::System:
         return processAdminMessage(env);
      case bs::message::TerminalUsers::Settings:
         return processSettings(env);
      }
   }
   return true;
}

bool QtGuiAdapter::processSettings(const bs::message::Envelope &env)
{
   SettingsMessage msg;
   if (!msg.ParseFromString(env.message)) {
      logger_->error("[{}] failed to parse settings msg #{}", __func__, env.id);
      return true;
   }
   switch (msg.data_case()) {
   case SettingsMessage::kGetResponse:
      return processSettingsGetResponse(msg.get_response());
   default: break;
   }
   return true;
}

bool QtGuiAdapter::processSettingsGetResponse(const SettingsMessage_SettingsResponse &response)
{
   for (const auto &setting : response.responses()) {
      switch (setting.request().index()) {
      case SetIdx_GUI_MainGeom: {
         QRect mainGeometry(setting.rect().left(), setting.rect().top()
            , setting.rect().width(), setting.rect().height());
         if (splashScreen_) {
            const auto &currentDisplay = getDisplay(mainGeometry.center());
            auto splashGeometry = splashScreen_->geometry();
            splashGeometry.moveCenter(currentDisplay->geometry().center());
            QMetaObject::invokeMethod(splashScreen_, [ss=splashScreen_, splashGeometry] {
               ss->setGeometry(splashGeometry);
            });
         }
         QMetaObject::invokeMethod(splashScreen_, [mw=mainWindow_, mainGeometry] {
            mw->onGetGeometry(mainGeometry);
         });
      }
      break;

      case SetIdx_Initialized:
         if (!setting.b()) {
#ifdef _WIN32
            // Read registry value in case it was set with installer. Could be used only on Windows for now.
            QSettings settings(QLatin1String("HKEY_CURRENT_USER\\Software\\blocksettle\\blocksettle"), QSettings::NativeFormat);
            bool showLicense = !settings.value(QLatin1String("license_accepted"), false).toBool();
#else
            bool showLicense = true;
#endif // _WIN32
            QMetaObject::invokeMethod(mainWindow_, [mw = mainWindow_, showLicense] {
               mw->showStartupDialog(showLicense);
            });
         }
         break;
      default: break;
      }
   }
   return true;
}

bool QtGuiAdapter::processAdminMessage(const bs::message::Envelope &env)
{
   AdministrativeMessage msg;
   if (!msg.ParseFromString(env.message)) {
      logger_->error("[{}] failed to parse admin msg #{}", __func__, env.id);
      return true;
   }
   switch (msg.data_case()) {
   case AdministrativeMessage::kComponentCreated:
      createdComponents_.insert(msg.component_created());
      break;
   case AdministrativeMessage::kComponentLoading:
      loadingComponents_.insert(msg.component_loading());
      break;
   default: break;
   }
   updateSplashProgress();
   return true;
}

void QtGuiAdapter::updateSplashProgress()
{
   if (!splashScreen_ || createdComponents_.empty()) {
      return;
   }
   int percent = 100 * loadingComponents_.size() / createdComponents_.size();
   QMetaObject::invokeMethod(splashScreen_, [this, percent] {
      splashScreen_->SetProgress(percent);
   });
   if (percent >= 100) {
      splashProgressCompleted();
   }
}

void QtGuiAdapter::splashProgressCompleted()
{
   if (!splashScreen_) {
      return;
   }
   QMetaObject::invokeMethod(splashScreen_, [this] {
      mainWindow_->show();
      loadingComponents_.clear();
      QTimer::singleShot(500, [this] {
         if (splashScreen_) {
            splashScreen_->hide();
            splashScreen_->deleteLater();
            splashScreen_ = nullptr;
         }
      });
   });
}

void QtGuiAdapter::requestInitialSettings()
{
   SettingsMessage msg;
   auto msgReq = msg.mutable_get_request();
   auto setReq = msgReq->add_requests();
   setReq->set_source(SettingSource_Local);
   setReq->set_index(SetIdx_GUI_MainGeom);
   setReq->set_type(SettingType_Rect);

   setReq = msgReq->add_requests();
   setReq->set_source(SettingSource_Local);
   setReq->set_index(SetIdx_Initialized);
   setReq->set_type(SettingType_Bool);

   bs::message::Envelope env{ 0, user_, userSettings_, bs::message::TimeStamp{}
      , bs::message::TimeStamp{}, msg.SerializeAsString(), true };
   pushFill(env);
}

#include "QtGuiAdapter.moc"
