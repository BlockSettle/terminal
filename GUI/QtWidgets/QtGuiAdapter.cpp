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
#include "Address.h"
#include "AppNap.h"
#include "BSMessageBox.h"
#include "BSTerminalSplashScreen.h"
#include "MainWindow.h"

#include "common.pb.h"
#include "terminal.pb.h"

using namespace BlockSettle::Common;
using namespace BlockSettle::Terminal;
using namespace bs::message;

Q_DECLARE_METATYPE(bs::error::AuthAddressSubmitResult)
Q_DECLARE_METATYPE(std::string)
Q_DECLARE_METATYPE(std::vector<bs::Address>)

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
   , userSettings_(std::make_shared<UserTerminal>(TerminalUsers::Settings))
   , userWallets_(std::make_shared<UserTerminal>(TerminalUsers::Wallets))
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
   qRegisterMetaType<std::vector<bs::Address>>();

   QString logoIcon;
   logoIcon = QLatin1String(":/SPLASH_LOGO");

   QPixmap splashLogo(logoIcon);
   const int splashScreenWidth = 400;
   splashScreen_ = new BSTerminalSplashScreen(splashLogo.scaledToWidth(splashScreenWidth
      , Qt::SmoothTransformation));
   updateSplashProgress();
   splashScreen_->show();

   mainWindow_ = new bs::gui::qt::MainWindow(logger_, queue_, user_);
   makeMainWinConnections();
   updateStates();

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

bool QtGuiAdapter::process(const Envelope &env)
{
   if (std::dynamic_pointer_cast<UserTerminal>(env.sender)) {
      switch (env.sender->value<bs::message::TerminalUsers>()) {
      case TerminalUsers::System:
         return processAdminMessage(env);
      case TerminalUsers::Settings:
         return processSettings(env);
      case TerminalUsers::Blockchain:
         return processBlockchain(env);
      case TerminalUsers::Signer:
         return processSigner(env);
      case TerminalUsers::Wallets:
         return processWallets(env);
      case TerminalUsers::AuthEid:
         return processAuthEid(env);
      case TerminalUsers::OnChainTracker:
         return processOnChainTrack(env);
      default:    break;
      }
   }
   return true;
}

bool QtGuiAdapter::processSettings(const Envelope &env)
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

bool QtGuiAdapter::processAdminMessage(const Envelope &env)
{
   AdministrativeMessage msg;
   if (!msg.ParseFromString(env.message)) {
      logger_->error("[{}] failed to parse admin msg #{}", __func__, env.id);
      return true;
   }
   switch (msg.data_case()) {
   case AdministrativeMessage::kComponentCreated:
      switch (static_cast<TerminalUsers>(msg.component_created())) {
      case TerminalUsers::API:
      case TerminalUsers::Settings:
         break;
      default:
         createdComponents_.insert(msg.component_created());
         break;
      }
      break;
   case AdministrativeMessage::kComponentLoading:
      loadingComponents_.insert(msg.component_loading());
      break;
   default: break;
   }
   updateSplashProgress();
   return true;
}

bool QtGuiAdapter::processBlockchain(const Envelope &env)
{
   ArmoryMessage msg;
   if (!msg.ParseFromString(env.message)) {
      logger_->error("[QtGuiAdapter::processBlockchain] failed to parse msg #{}"
         , env.id);
      if (!env.receiver) {
         logger_->debug("[{}] no receiver", __func__);
      }
      return true;
   }
   switch (msg.data_case()) {
   case ArmoryMessage::kLoading:
      loadingComponents_.insert(env.sender->value());
      updateSplashProgress();
      break;
   case ArmoryMessage::kStateChanged:
      armoryState_ = msg.state_changed().state();
      blockNum_ = msg.state_changed().top_block();
      if (mainWindow_) {
         QMetaObject::invokeMethod(mainWindow_, [this, state=msg.state_changed()] {
            mainWindow_->onArmoryStateChanged(state.state(), state.top_block());
         });
      }
      break;
   case ArmoryMessage::kNewBlock:
      blockNum_ = msg.new_block().top_block();
      if (mainWindow_) {
         QMetaObject::invokeMethod(mainWindow_, [this]{
            mainWindow_->onArmoryStateChanged(armoryState_, blockNum_);
            });
      }
      break;
   default:    break;
   }
   return true;
}

bool QtGuiAdapter::processSigner(const Envelope &env)
{
   SignerMessage msg;
   if (!msg.ParseFromString(env.message)) {
      logger_->error("[QtGuiAdapter::processSigner] failed to parse msg #{}"
         , env.id);
      if (!env.receiver) {
         logger_->debug("[{}] no receiver", __func__);
      }
      return true;
   }
   switch (msg.data_case()) {
   case SignerMessage::kState:
      signerState_ = msg.state().code();
      signerDetails_ = msg.state().text();
      if (mainWindow_) {
         QMetaObject::invokeMethod(mainWindow_, [this]{
            mainWindow_->onSignerStateChanged(signerState_, signerDetails_);
            });
      }
      break;
   case SignerMessage::kNeedNewWalletPrompt:
      createWallet(true);
      break;
   default:    break;
   }
   return true;
}

bool QtGuiAdapter::processWallets(const Envelope &env)
{
   WalletsMessage msg;
   if (!msg.ParseFromString(env.message)) {
      logger_->error("[{}] failed to parse msg #{}", __func__, env.id);
      return true;
   }
   switch (msg.data_case()) {
   case WalletsMessage::kLoading:
      loadingComponents_.insert(env.sender->value());
      updateSplashProgress();
      break;
   case WalletsMessage::kWalletLoaded: {
      const auto &wi = bs::sync::WalletInfo::fromCommonMsg(msg.wallet_loaded());
      processWalletLoaded(wi);
   }
      break;
   case WalletsMessage::kHdWallet: {
      const auto &hdw = bs::sync::HDWalletData::fromCommonMessage(msg.hd_wallet());
      QMetaObject::invokeMethod(mainWindow_, [this, hdw] {
         mainWindow_->onHDWalletDetails(hdw);
      });
   }
      break;
   default:    break;
   }
   return true;
}

bool QtGuiAdapter::processAuthEid(const Envelope &env)
{
   AuthEidMessage msg;
   if (!msg.ParseFromString(env.message)) {
      logger_->error("[{}] failed to parse msg #{}", __func__, env.id);
      return true;
   }
   switch (msg.data_case()) {
   case AuthEidMessage::kLoading:
      loadingComponents_.insert(env.sender->value());
      updateSplashProgress();
      break;
   default:    break;
   }
   return true;
}

bool QtGuiAdapter::processOnChainTrack(const Envelope &env)
{
   OnChainTrackMessage msg;
   if (!msg.ParseFromString(env.message)) {
      logger_->error("[{}] failed to parse msg #{}", __func__, env.id);
      return true;
   }
   switch (msg.data_case()) {
   case OnChainTrackMessage::kLoading:
      loadingComponents_.insert(env.sender->value());
      updateSplashProgress();
      break;
   default:    break;
   }
   return true;
}

void QtGuiAdapter::updateStates()
{
   if (!mainWindow_) {
      return;
   }
   if (armoryState_ >= 0) {
      mainWindow_->onArmoryStateChanged(armoryState_, blockNum_);
   }
   if (signerState_ >= 0) {
      mainWindow_->onSignerStateChanged(signerState_, signerDetails_);
   }
   for (const auto &hdWallet : hdWallets_) {
      mainWindow_->onHDWallet(hdWallet.second);
   }
}

void QtGuiAdapter::updateSplashProgress()
{
   if (!splashScreen_ || createdComponents_.empty()) {
      return;
   }
/*   std::string l, c;
   for (const auto &lc : loadingComponents_) {
      l += std::to_string(lc) + " ";
   }
   for (const auto &cc : createdComponents_) {
      c += std::to_string(cc) + " ";
   }
   logger_->debug("[{}] {}/{}", __func__, l, c);*/
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
   loadingComponents_.clear();

   QMetaObject::invokeMethod(splashScreen_, [this] {
      mainWindow_->show();
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

   Envelope env{ 0, user_, userSettings_, {}, {}, msg.SerializeAsString(), true };
   pushFill(env);
}

void QtGuiAdapter::makeMainWinConnections()
{
   connect(mainWindow_, &bs::gui::qt::MainWindow::needHDWalletDetails, this, &QtGuiAdapter::onNeedHDWalletDetails);
   connect(mainWindow_, &bs::gui::qt::MainWindow::needExtAddresses, this, &QtGuiAdapter::onNeedExtAddresses);
   connect(mainWindow_, &bs::gui::qt::MainWindow::needIntAddresses, this, &QtGuiAdapter::onNeedIntAddresses);
   connect(mainWindow_, &bs::gui::qt::MainWindow::needUsedAddresses, this, &QtGuiAdapter::onNeedUsedAddresses);
   connect(mainWindow_, &bs::gui::qt::MainWindow::needAddrComments, this, &QtGuiAdapter::onNeedAddrComments);
}

void QtGuiAdapter::createWallet(bool primary)
{
   logger_->debug("[{}]", __func__);
}

void QtGuiAdapter::onNeedHDWalletDetails(const std::string &walletId)
{
   WalletsMessage msg;
   msg.set_hd_wallet_get(walletId);
   Envelope env{ 0, user_, userWallets_, {}, {}, msg.SerializeAsString(), true };
   pushFill(env);
}

void QtGuiAdapter::onNeedExtAddresses(std::string walletId)
{
   logger_->debug("[{}] {}", __func__, walletId);
}

void QtGuiAdapter::onNeedIntAddresses(std::string walletId)
{
   logger_->debug("[{}] {}", __func__, walletId);
}

void QtGuiAdapter::onNeedUsedAddresses(std::string walletId)
{
   logger_->debug("[{}] {}", __func__, walletId);
}

void QtGuiAdapter::onNeedAddrComments(std::string walletId, const std::vector<bs::Address> &)
{
   logger_->debug("[{}] {}", __func__, walletId);
}

void QtGuiAdapter::processWalletLoaded(const bs::sync::WalletInfo &wi)
{
   hdWallets_[wi.id] = wi;
   if (mainWindow_) {
      QMetaObject::invokeMethod(mainWindow_, [this, wi] {
         mainWindow_->onHDWallet(wi);
      });
   }
}


#include "QtGuiAdapter.moc"
