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
#include "BsClient.h"
#include "BSMessageBox.h"
#include "BSTerminalSplashScreen.h"
#include "MainWindow.h"
#include "MessageUtils.h"
#include "ProtobufHeadlessUtils.h"
#include "SettingsAdapter.h"
#include "TradesVerification.h"

#include "common.pb.h"
#include "terminal.pb.h"

using namespace BlockSettle::Common;
using namespace BlockSettle::Terminal;
using namespace bs::message;

Q_DECLARE_METATYPE(bs::error::AuthAddressSubmitResult)
Q_DECLARE_METATYPE(std::string)
Q_DECLARE_METATYPE(std::vector<bs::Address>)
Q_DECLARE_METATYPE(std::vector<ApplicationSettings::Setting>);
Q_DECLARE_METATYPE(bs::PayoutSignatureType)

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
   , userBlockchain_(std::make_shared<UserTerminal>(TerminalUsers::Blockchain))
   , userSigner_(std::make_shared<UserTerminal>(TerminalUsers::Signer))
   , userBS_(std::make_shared<UserTerminal>(TerminalUsers::BsServer))
   , userMatch_(std::make_shared<UserTerminal>(TerminalUsers::Matching))
   , userSettl_(std::make_shared<UserTerminal>(TerminalUsers::Settlement))
   , userMD_(std::make_shared<UserTerminal>(TerminalUsers::MktData))
   , userTrk_(std::make_shared<UserTerminal>(TerminalUsers::OnChainTracker))
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
   qRegisterMetaType<std::vector<ApplicationSettings::Setting>>();

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
      case TerminalUsers::BsServer:
         return processBsServer(env);
      case TerminalUsers::Settlement:
         return processSettlement(env);
      case TerminalUsers::Matching:
         return processMatching(env);
      case TerminalUsers::MktData:
         return processMktData(env);
      case TerminalUsers::OnChainTracker:
         return processOnChainTrack(env);
      case TerminalUsers::Assets:
         return processAssets(env);
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
   case SettingsMessage::kSettingsUpdated:
      return processSettingsGetResponse(msg.settings_updated());
   case SettingsMessage::kState:
      return processSettingsState(msg.state());
   case SettingsMessage::kArmoryServers:
      return processArmoryServers(msg.armory_servers());
   case SettingsMessage::kSignerServers:
      return processSignerServers(msg.signer_servers());
   default: break;
   }
   return true;
}

bool QtGuiAdapter::processSettingsGetResponse(const SettingsMessage_SettingsResponse &response)
{
   std::map<int, QVariant> settings;
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
         QMetaObject::invokeMethod(mainWindow_, [mw=mainWindow_, mainGeometry] {
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
            onResetSettings({});
         }
         break;

      default:
         settings[setting.request().index()] = fromResponse(setting);
         break;
      }
   }
   if (!settings.empty()) {
      return QMetaObject::invokeMethod(mainWindow_, [mw = mainWindow_, settings] {
         for (const auto& setting : settings) {
            mw->onSetting(setting.first, setting.second);
         }
      });
   }
   return true;
}

bool QtGuiAdapter::processSettingsState(const SettingsMessage_SettingsResponse& response)
{
   ApplicationSettings::State state;
   for (const auto& setting : response.responses()) {
      state[static_cast<ApplicationSettings::Setting>(setting.request().index())] =
         fromResponse(setting);
   }
   return QMetaObject::invokeMethod(mainWindow_, [mw = mainWindow_, state] {
      mw->onSettingsState(state);
   });
}

bool QtGuiAdapter::processArmoryServers(const SettingsMessage_ArmoryServers& response)
{
   QList<ArmoryServer> servers;
   for (const auto& server : response.servers()) {
      servers << ArmoryServer{ QString::fromStdString(server.server_name())
         , static_cast<NetworkType>(server.network_type())
         , QString::fromStdString(server.server_address())
         , std::stoi(server.server_port()), QString::fromStdString(server.server_key())
         , SecureBinaryData::fromString(server.password())
         , server.run_locally(), server.one_way_auth() };
   }
   logger_->debug("[{}] {} servers, cur: {}, conn: {}", __func__, servers.size()
      , response.idx_current(), response.idx_connected());
   return QMetaObject::invokeMethod(mainWindow_, [mw = mainWindow_, servers, response] {
      mw->onArmoryServers(servers, response.idx_current(), response.idx_connected());
   });
}

bool QtGuiAdapter::processSignerServers(const SettingsMessage_SignerServers& response)
{
   QList<SignerHost> servers;
   for (const auto& server : response.servers()) {
      servers << SignerHost{ QString::fromStdString(server.name())
         , QString::fromStdString(server.host()), std::stoi(server.port())
         , QString::fromStdString(server.key()) };
   }
   return QMetaObject::invokeMethod(mainWindow_, [mw = mainWindow_, servers, response] {
      mw->onSignerSettings(servers, response.own_key(), response.idx_current());
   });
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
            mainWindow_->onNewBlock(armoryState_, blockNum_);
         });
      }
      break;
   case ArmoryMessage::kWalletRegistered:
      if (msg.wallet_registered().success() && msg.wallet_registered().wallet_id().empty()) {
         walletsReady_ = true;
         QMetaObject::invokeMethod(mainWindow_, [this] {
            mainWindow_->onWalletsReady();
         });
      }
   case ArmoryMessage::kLedgerEntries:
      return processLedgerEntries(msg.ledger_entries());
   case ArmoryMessage::kAddressHistory:
      return processAddressHist(msg.address_history());
   case ArmoryMessage::kFeeLevelsResponse:
      return processFeeLevels(msg.fee_levels_response());
   case ArmoryMessage::kZcReceived:
      return processZC(msg.zc_received());
   case ArmoryMessage::kZcInvalidated:
      return processZCInvalidated(msg.zc_invalidated());
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
   case SignerMessage::kSignTxResponse:
      return processSignTX(msg.sign_tx_response());
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

   case WalletsMessage::kWalletAddresses: {
      std::vector<bs::sync::Address> addresses;
      for (const auto &addr : msg.wallet_addresses().addresses()) {
         try {
            addresses.push_back({ std::move(bs::Address::fromAddressString(addr.address()))
               , addr.index(), addr.wallet_id() });
         }
         catch (const std::exception &) {}
      }
      QMetaObject::invokeMethod(mainWindow_, [this, addresses] {
         mainWindow_->onAddresses(addresses);
      });
   }
      break;

   case WalletsMessage::kAddrComments: {
      std::map<bs::Address, std::string> comments;
      for (const auto &addrComment : msg.addr_comments().comments()) {
         try {
            comments[bs::Address::fromAddressString(addrComment.address())] = addrComment.comment();
         }
         catch (const std::exception &) {}
      }
      QMetaObject::invokeMethod(mainWindow_, [this, comments, walletId = msg.addr_comments().wallet_id()]{
         mainWindow_->onAddressComments(walletId, comments);
      });
   }
      break;
   case WalletsMessage::kWalletData:
      return processWalletData(env.id, msg.wallet_data());
   case WalletsMessage::kWalletBalances:
      return processWalletBalances(env, msg.wallet_balances());
   case WalletsMessage::kTxDetailsResponse:
      return processTXDetails(env.id, msg.tx_details_response());
   case WalletsMessage::kWalletsListResponse:
      return processWalletsList(msg.wallets_list_response());
   case WalletsMessage::kUtxos:
      return processUTXOs(msg.utxos());
   case WalletsMessage::kAuthWallet:
      return processAuthWallet(msg.auth_wallet());
   case WalletsMessage::kAuthKey:
      return processAuthKey(msg.auth_key());
   case WalletsMessage::kReservedUtxos:
      return processReservedUTXOs(msg.reserved_utxos());
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
   case OnChainTrackMessage::kAuthState:
      return processAuthState(msg.auth_state());
   case OnChainTrackMessage::kVerifiedAuthAddresses:
      return processVerifiedAuthAddrs(msg.verified_auth_addresses());
   default:    break;
   }
   return true;
}

bool QtGuiAdapter::processAssets(const bs::message::Envelope& env)
{
   AssetsMessage msg;
   if (!msg.ParseFromString(env.message)) {
      logger_->error("[{}] failed to parse msg #{}", __func__, env.id);
      return true;
   }
   switch (msg.data_case()) {
   case AssetsMessage::kSubmittedAuthAddrs:
      return processSubmittedAuthAddrs(msg.submitted_auth_addrs());
   case AssetsMessage::kBalance:
      return processBalance(msg.balance());
   default: break;
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
   if (walletsReady_) {
      mainWindow_->onWalletsReady();
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

   setReq = msgReq->add_requests();
   setReq->set_source(SettingSource_Local);
   setReq->set_index(SetIdx_GUI_MainTab);
   setReq->set_type(SettingType_Int);

   setReq = msgReq->add_requests();
   setReq->set_source(SettingSource_Local);
   setReq->set_index(SetIdx_ShowInfoWidget);
   setReq->set_type(SettingType_Bool);

   setReq = msgReq->add_requests();
   setReq->set_source(SettingSource_Local);
   setReq->set_index(SetIdx_AdvancedTXisDefault);
   setReq->set_type(SettingType_Bool);

   setReq = msgReq->add_requests();
   setReq->set_source(SettingSource_Local);
   setReq->set_index(SetIdx_CloseToTray);
   setReq->set_type(SettingType_Bool);

   setReq = msgReq->add_requests();
   setReq->set_source(SettingSource_Local);
   setReq->set_index(SetIdx_Environment);
   setReq->set_type(SettingType_Int);

   Envelope env{ 0, user_, userSettings_, {}, {}, msg.SerializeAsString(), true };
   pushFill(env);
}

void QtGuiAdapter::makeMainWinConnections()
{
   connect(mainWindow_, &bs::gui::qt::MainWindow::getSettings, this, &QtGuiAdapter::onGetSettings);
   connect(mainWindow_, &bs::gui::qt::MainWindow::putSetting, this, &QtGuiAdapter::onPutSetting);
   connect(mainWindow_, &bs::gui::qt::MainWindow::resetSettings, this, &QtGuiAdapter::onResetSettings);
   connect(mainWindow_, &bs::gui::qt::MainWindow::resetSettingsToState, this, &QtGuiAdapter::onResetSettingsToState);
   connect(mainWindow_, &bs::gui::qt::MainWindow::needSettingsState, this, &QtGuiAdapter::onNeedSettingsState);
   connect(mainWindow_, &bs::gui::qt::MainWindow::needArmoryServers, this, &QtGuiAdapter::onNeedArmoryServers);
   connect(mainWindow_, &bs::gui::qt::MainWindow::setArmoryServer, this, &QtGuiAdapter::onSetArmoryServer);
   connect(mainWindow_, &bs::gui::qt::MainWindow::addArmoryServer, this, &QtGuiAdapter::onAddArmoryServer);
   connect(mainWindow_, &bs::gui::qt::MainWindow::delArmoryServer, this, &QtGuiAdapter::onDelArmoryServer);
   connect(mainWindow_, &bs::gui::qt::MainWindow::updArmoryServer, this, &QtGuiAdapter::onUpdArmoryServer);
   connect(mainWindow_, &bs::gui::qt::MainWindow::needArmoryReconnect, this, &QtGuiAdapter::onNeedArmoryReconnect);
   connect(mainWindow_, &bs::gui::qt::MainWindow::needSigners, this, &QtGuiAdapter::onNeedSigners);
   connect(mainWindow_, &bs::gui::qt::MainWindow::setSigner, this, &QtGuiAdapter::onSetSigner);
   connect(mainWindow_, &bs::gui::qt::MainWindow::bootstrapDataLoaded, this, &QtGuiAdapter::onBootstrapDataLoaded);
   connect(mainWindow_, &bs::gui::qt::MainWindow::needHDWalletDetails, this, &QtGuiAdapter::onNeedHDWalletDetails);
   connect(mainWindow_, &bs::gui::qt::MainWindow::needWalletData, this, &QtGuiAdapter::onNeedWalletData);
   connect(mainWindow_, &bs::gui::qt::MainWindow::needWalletBalances, this, &QtGuiAdapter::onNeedWalletBalances);
   connect(mainWindow_, &bs::gui::qt::MainWindow::needExtAddresses, this, &QtGuiAdapter::onNeedExtAddresses);
   connect(mainWindow_, &bs::gui::qt::MainWindow::needIntAddresses, this, &QtGuiAdapter::onNeedIntAddresses);
   connect(mainWindow_, &bs::gui::qt::MainWindow::needUsedAddresses, this, &QtGuiAdapter::onNeedUsedAddresses);
   connect(mainWindow_, &bs::gui::qt::MainWindow::needAddrComments, this, &QtGuiAdapter::onNeedAddrComments);
   connect(mainWindow_, &bs::gui::qt::MainWindow::setAddrComment, this, &QtGuiAdapter::onSetAddrComment);
   connect(mainWindow_, &bs::gui::qt::MainWindow::needLedgerEntries, this, &QtGuiAdapter::onNeedLedgerEntries);
   connect(mainWindow_, &bs::gui::qt::MainWindow::needTXDetails, this, &QtGuiAdapter::onNeedTXDetails);
   connect(mainWindow_, &bs::gui::qt::MainWindow::needAddressHistory, this, &QtGuiAdapter::onNeedAddressHistory);
   connect(mainWindow_, &bs::gui::qt::MainWindow::needWalletsList, this, &QtGuiAdapter::onNeedWalletsList);
   connect(mainWindow_, &bs::gui::qt::MainWindow::needFeeLevels, this, &QtGuiAdapter::onNeedFeeLevels);
   connect(mainWindow_, &bs::gui::qt::MainWindow::needUTXOs, this, &QtGuiAdapter::onNeedUTXOs);
   connect(mainWindow_, &bs::gui::qt::MainWindow::needSignTX, this, &QtGuiAdapter::onNeedSignTX);
   connect(mainWindow_, &bs::gui::qt::MainWindow::needBroadcastZC, this, &QtGuiAdapter::onNeedBroadcastZC);
   connect(mainWindow_, &bs::gui::qt::MainWindow::needSetTxComment, this, &QtGuiAdapter::onNeedSetTxComment);
   connect(mainWindow_, &bs::gui::qt::MainWindow::needOpenBsConnection, this, &QtGuiAdapter::onNeedOpenBsConnection);
   connect(mainWindow_, &bs::gui::qt::MainWindow::needCloseBsConnection, this, &QtGuiAdapter::onNeedCloseBsConnection);
   connect(mainWindow_, &bs::gui::qt::MainWindow::needStartLogin, this, &QtGuiAdapter::onNeedStartLogin);
   connect(mainWindow_, &bs::gui::qt::MainWindow::needCancelLogin, this, &QtGuiAdapter::onNeedCancelLogin);
   connect(mainWindow_, &bs::gui::qt::MainWindow::needMatchingLogin, this, &QtGuiAdapter::onNeedMatchingLogin);
   connect(mainWindow_, &bs::gui::qt::MainWindow::needMatchingLogout, this, &QtGuiAdapter::onNeedMatchingLogout);
   connect(mainWindow_, &bs::gui::qt::MainWindow::needSetUserId, this, &QtGuiAdapter::onNeedSetUserId);
   connect(mainWindow_, &bs::gui::qt::MainWindow::setRecommendedFeeRate, this, &QtGuiAdapter::onSetRecommendedFeeRate);
   connect(mainWindow_, &bs::gui::qt::MainWindow::needMdConnection, this, &QtGuiAdapter::onNeedMdConnection);
   connect(mainWindow_, &bs::gui::qt::MainWindow::needNewAuthAddress, this, &QtGuiAdapter::onNeedNewAuthAddress);
   connect(mainWindow_, &bs::gui::qt::MainWindow::needSubmitAuthAddress, this, &QtGuiAdapter::onNeedSubmitAuthAddress);
   connect(mainWindow_, &bs::gui::qt::MainWindow::needSubmitRFQ, this, &QtGuiAdapter::onNeedSubmitRFQ);
   connect(mainWindow_, &bs::gui::qt::MainWindow::needAcceptRFQ, this, &QtGuiAdapter::onNeedAcceptRFQ);
   connect(mainWindow_, &bs::gui::qt::MainWindow::needCancelRFQ, this, &QtGuiAdapter::onNeedCancelRFQ);
   connect(mainWindow_, &bs::gui::qt::MainWindow::needAuthKey, this, &QtGuiAdapter::onNeedAuthKey);
   connect(mainWindow_, &bs::gui::qt::MainWindow::needReserveUTXOs, this, &QtGuiAdapter::onNeedReserveUTXOs);
   connect(mainWindow_, &bs::gui::qt::MainWindow::needUnreserveUTXOs, this, &QtGuiAdapter::onNeedUnreserveUTXOs);
}

void QtGuiAdapter::onGetSettings(const std::vector<ApplicationSettings::Setting>& settings)
{
   SettingsMessage msg;
   auto msgReq = msg.mutable_get_request();
   for (const auto& setting : settings) {
      auto setReq = msgReq->add_requests();
      setReq->set_source(SettingSource_Local);
      setReq->set_index(static_cast<SettingIndex>(setting));
   }
   Envelope env{ 0, user_, userSettings_, {}, {}, msg.SerializeAsString(), true };
   pushFill(env);
}

void QtGuiAdapter::onPutSetting(ApplicationSettings::Setting idx, const QVariant &value)
{
   SettingsMessage msg;
   auto msgReq = msg.mutable_put_request();
   auto setResp = msgReq->add_responses();
   auto setReq = setResp->mutable_request();
   setReq->set_source(SettingSource_Local);
   setReq->set_index(static_cast<SettingIndex>(idx));
   setFromQVariant(value, setReq, setResp);

   Envelope env{ 0, user_, userSettings_, {}, {}, msg.SerializeAsString(), true };
   pushFill(env);
}

void QtGuiAdapter::onResetSettings(const std::vector<ApplicationSettings::Setting>& settings)
{
   SettingsMessage msg;
   auto msgResp = msg.mutable_reset();
   for (const auto& setting : settings) {
      auto msgReq = msgResp->add_requests();
      msgReq->set_index(static_cast<SettingIndex>(setting));
      msgReq->set_source(SettingSource_Local);
   }
   Envelope env{ 0, user_, userSettings_, {}, {}, msg.SerializeAsString(), true };
   pushFill(env);
}

void QtGuiAdapter::onResetSettingsToState(const ApplicationSettings::State& state)
{
   SettingsMessage msg;
   auto msgResp = msg.mutable_reset_to_state();
   for (const auto& st : state) {
      auto setResp = msgResp->add_responses();
      auto setReq = setResp->mutable_request();
      setReq->set_source(SettingSource_Local);
      setReq->set_index(static_cast<SettingIndex>(st.first));
      setFromQVariant(st.second, setReq, setResp);
   }
   Envelope env{ 0, user_, userSettings_, {}, {}, msg.SerializeAsString(), true };
   pushFill(env);
}

void QtGuiAdapter::onNeedSettingsState()
{
   SettingsMessage msg;
   msg.mutable_state_get();
   Envelope env{ 0, user_, userSettings_, {}, {}, msg.SerializeAsString(), true };
   pushFill(env);
}

void QtGuiAdapter::onNeedArmoryServers()
{
   SettingsMessage msg;
   msg.mutable_armory_servers_get();
   Envelope env{ 0, user_, userSettings_, {}, {}, msg.SerializeAsString(), true };
   pushFill(env);
}

void QtGuiAdapter::onSetArmoryServer(int index)
{
   SettingsMessage msg;
   msg.set_set_armory_server(index);
   Envelope env{ 0, user_, userSettings_, {}, {}, msg.SerializeAsString(), true };
   pushFill(env);
}

void QtGuiAdapter::onAddArmoryServer(const ArmoryServer& server)
{
   SettingsMessage msg;
   auto msgReq = msg.mutable_add_armory_server();
   msgReq->set_network_type((int)server.netType);
   msgReq->set_server_name(server.name.toStdString());
   msgReq->set_server_address(server.armoryDBIp.toStdString());
   msgReq->set_server_port(std::to_string(server.armoryDBPort));
   msgReq->set_server_key(server.armoryDBKey.toStdString());
   msgReq->set_run_locally(server.runLocally);
   msgReq->set_one_way_auth(server.oneWayAuth_);
   msgReq->set_password(server.password.toBinStr());
   Envelope env{ 0, user_, userSettings_, {}, {}, msg.SerializeAsString(), true };
   pushFill(env);
}

void QtGuiAdapter::onDelArmoryServer(int index)
{
   SettingsMessage msg;
   msg.set_del_armory_server(index);
   Envelope env{ 0, user_, userSettings_, {}, {}, msg.SerializeAsString(), true };
   pushFill(env);
}

void QtGuiAdapter::onUpdArmoryServer(int index, const ArmoryServer& server)
{
   SettingsMessage msg;
   auto msgReq = msg.mutable_upd_armory_server();
   msgReq->set_index(index);
   auto msgSrv = msgReq->mutable_server();
   msgSrv->set_network_type((int)server.netType);
   msgSrv->set_server_name(server.name.toStdString());
   msgSrv->set_server_address(server.armoryDBIp.toStdString());
   msgSrv->set_server_port(std::to_string(server.armoryDBPort));
   msgSrv->set_server_key(server.armoryDBKey.toStdString());
   msgSrv->set_run_locally(server.runLocally);
   msgSrv->set_one_way_auth(server.oneWayAuth_);
   msgSrv->set_password(server.password.toBinStr());
   Envelope env{ 0, user_, userSettings_, {}, {}, msg.SerializeAsString(), true };
   pushFill(env);
}

void QtGuiAdapter::createWallet(bool primary)
{
   logger_->debug("[{}] primary: {}", __func__, primary);
}

void QtGuiAdapter::onNeedHDWalletDetails(const std::string &walletId)
{
   WalletsMessage msg;
   msg.set_hd_wallet_get(walletId);
   Envelope env{ 0, user_, userWallets_, {}, {}, msg.SerializeAsString(), true };
   pushFill(env);
}

void QtGuiAdapter::onNeedWalletBalances(const std::string &walletId)
{
   logger_->debug("[{}] {}", __func__, walletId);
   WalletsMessage msg;
   msg.set_get_wallet_balances(walletId);
   Envelope env{ 0, user_, userWallets_, {}, {}, msg.SerializeAsString(), true };
   pushFill(env);
}

void QtGuiAdapter::onNeedWalletData(const std::string& walletId)
{
   WalletsMessage msg;
   msg.set_wallet_get(walletId);
   Envelope env{ 0, user_, userWallets_, {}, {}, msg.SerializeAsString(), true };
   if (pushFill(env)) {
      walletGetMap_[env.id] = walletId;
   }
}

void QtGuiAdapter::onNeedExtAddresses(const std::string &walletId)
{
   logger_->debug("[{}] {}", __func__, walletId);
   WalletsMessage msg;
   msg.set_get_ext_addresses(walletId);
   Envelope env{ 0, user_, userWallets_, {}, {}, msg.SerializeAsString(), true };
   pushFill(env);
}

void QtGuiAdapter::onNeedIntAddresses(const std::string &walletId)
{
   logger_->debug("[{}] {}", __func__, walletId);
   WalletsMessage msg;
   msg.set_get_int_addresses(walletId);
   Envelope env{ 0, user_, userWallets_, {}, {}, msg.SerializeAsString(), true };
   pushFill(env);
}

void QtGuiAdapter::onNeedUsedAddresses(const std::string &walletId)
{
   logger_->debug("[{}] {}", __func__, walletId);
   WalletsMessage msg;
   msg.set_get_used_addresses(walletId);
   Envelope env{ 0, user_, userWallets_, {}, {}, msg.SerializeAsString(), true };
   pushFill(env);
}

void QtGuiAdapter::onNeedAddrComments(const std::string &walletId
   , const std::vector<bs::Address> &addrs)
{
   WalletsMessage msg;
   auto msgReq = msg.mutable_get_addr_comments();
   msgReq->set_wallet_id(walletId);
   for (const auto &addr : addrs) {
      auto addrReq = msgReq->add_addresses();
      addrReq->set_address(addr.display());
   }
   Envelope env{ 0, user_, userWallets_, {}, {}, msg.SerializeAsString(), true };
   pushFill(env);
}

void QtGuiAdapter::onSetAddrComment(const std::string &walletId, const bs::Address &addr
   , const std::string &comment)
{
   WalletsMessage msg;
   auto msgReq = msg.mutable_set_addr_comments();
   msgReq->set_wallet_id(walletId);
   auto msgComm = msgReq->add_comments();
   msgComm->set_address(addr.display());
   msgComm->set_comment(comment);
   Envelope env{ 0, user_, userWallets_, {}, {}, msg.SerializeAsString(), true };
   pushFill(env);
}

void QtGuiAdapter::onNeedLedgerEntries(const std::string &filter)
{
   ArmoryMessage msg;
   msg.set_get_ledger_entries(filter);
   Envelope env{ 0, user_, userBlockchain_, {}, {}, msg.SerializeAsString(), true };
   pushFill(env);
}

void QtGuiAdapter::onNeedTXDetails(const const std::vector<bs::sync::TXWallet> &txWallet
   , bool useCache, const bs::Address &addr)
{
   WalletsMessage msg;
   auto msgReq = msg.mutable_tx_details_request();
   for (const auto &txw : txWallet) {
      auto request = msgReq->add_requests();
      request->set_tx_hash(txw.txHash.toBinStr());
      request->set_wallet_id(txw.walletId);
      request->set_value(txw.value);
   }
   if (!addr.empty()) {
      msgReq->set_address(addr.display());
   }
   msgReq->set_use_cache(useCache);
   Envelope env{ 0, user_, userWallets_, {}, {}, msg.SerializeAsString(), true };
   pushFill(env);
}

void QtGuiAdapter::onNeedAddressHistory(const bs::Address& addr)
{
   logger_->debug("[{}] {}", __func__, addr.display());
   ArmoryMessage msg;
   msg.set_get_address_history(addr.display());
   Envelope env{ 0, user_, userBlockchain_, {}, {}, msg.SerializeAsString(), true };
   pushFill(env);
}

void QtGuiAdapter::onNeedWalletsList(UiUtils::WalletsTypes wt, const std::string &id)
{
   WalletsMessage msg;
   auto msgReq = msg.mutable_wallets_list_request();
   msgReq->set_id(id);
   if (wt & UiUtils::WalletsTypes::WatchOnly) {
      msgReq->set_watch_only(true);
   }
   if (wt & UiUtils::WalletsTypes::Full) {
      msgReq->set_full(true);
   }
   if (wt & UiUtils::WalletsTypes::HardwareLegacy) {
      msgReq->set_legacy(true);
      msgReq->set_hardware(true);
   }
   if (wt & UiUtils::WalletsTypes::HardwareNativeSW) {
      msgReq->set_native_sw(true);
      msgReq->set_hardware(true);
   }
   if (wt & UiUtils::WalletsTypes::HardwareNestedSW) {
      msgReq->set_nested_sw(true);
      msgReq->set_hardware(true);
   }
   Envelope env{ 0, user_, userWallets_, {}, {}, msg.SerializeAsString(), true };
   pushFill(env);
}

void QtGuiAdapter::onNeedFeeLevels(const std::vector<unsigned int>& levels)
{
   ArmoryMessage msg;
   auto msgReq = msg.mutable_fee_levels_request();
   for (const auto& level : levels) {
      msgReq->add_levels(level);
   }
   Envelope env{ 0, user_, userBlockchain_, {}, {}, msg.SerializeAsString(), true };
   pushFill(env);
}

void QtGuiAdapter::onNeedUTXOs(const std::string& id, const std::string& walletId, bool confOnly, bool swOnly)
{
   WalletsMessage msg;
   auto msgReq = msg.mutable_get_utxos();
   msgReq->set_id(id);
   msgReq->set_wallet_id(walletId);
   msgReq->set_confirmed_only(confOnly);
   msgReq->set_segwit_only(swOnly);
   Envelope env{ 0, user_, userWallets_, {}, {}, msg.SerializeAsString(), true };
   pushFill(env);
}

void QtGuiAdapter::onNeedSignTX(const std::string& id
   , const bs::core::wallet::TXSignRequest& txReq, bool keepDupRecips
   , SignContainer::TXSignMode mode)
{
   logger_->debug("[{}] {}", __func__, id);
   SignerMessage msg;
   auto msgReq = msg.mutable_sign_tx_request();
   msgReq->set_id(id);
   *msgReq->mutable_tx_request() = bs::signer::coreTxRequestToPb(txReq, keepDupRecips);
   msgReq->set_sign_mode((int)mode);
   msgReq->set_keep_dup_recips(keepDupRecips);
   Envelope env{ 0, user_, userSigner_, {}, {}, msg.SerializeAsString(), true };
   pushFill(env);
}

void QtGuiAdapter::onNeedBroadcastZC(const std::string& id, const BinaryData& tx)
{
   ArmoryMessage msg;
   auto msgReq = msg.mutable_tx_push();
   msgReq->set_push_id(id);
   auto msgTx = msgReq->add_txs_to_push();
   msgTx->set_tx(tx.toBinStr());
   //not adding TX hashes atm
   Envelope env{ 0, user_, userBlockchain_, {}, {}, msg.SerializeAsString(), true };
   pushFill(env);
}

void QtGuiAdapter::onNeedArmoryReconnect()
{
   ArmoryMessage msg;
   msg.mutable_reconnect();
   Envelope env{ 0, user_, userBlockchain_, {}, {}, msg.SerializeAsString(), true };
   pushFill(env);
}

void QtGuiAdapter::onNeedSigners()
{
   SettingsMessage msg;
   msg.mutable_signer_servers_get();
   Envelope env{ 0, user_, userSettings_, {}, {}, msg.SerializeAsString(), true };
   pushFill(env);
}

void QtGuiAdapter::onSetSigner(int index)
{
   SettingsMessage msg;
   msg.set_set_signer_server(index);
   Envelope env{ 0, user_, userSettings_, {}, {}, msg.SerializeAsString(), true };
   pushFill(env);
}

void QtGuiAdapter::onNeedSetTxComment(const std::string& walletId, const BinaryData& txHash, const std::string& comment)
{
   logger_->debug("[{}] {}: {}", __func__, txHash.toHexStr(true), comment);
   WalletsMessage msg;
   auto msgReq = msg.mutable_set_tx_comment();
   msgReq->set_wallet_id(walletId);
   msgReq->set_tx_hash(txHash.toBinStr());
   msgReq->set_comment(comment);
   Envelope env{ 0, user_, userWallets_, {}, {}, msg.SerializeAsString(), true };
   pushFill(env);
}

void QtGuiAdapter::onNeedOpenBsConnection()
{
   BsServerMessage msg;
   msg.mutable_open_connection();
   Envelope env{ 0, user_, userBS_, {}, {}, msg.SerializeAsString(), true };
   pushFill(env);
}

void QtGuiAdapter::onNeedCloseBsConnection()
{
   BsServerMessage msg;
   msg.mutable_close_connection();
   Envelope env{ 0, user_, userBS_, {}, {}, msg.SerializeAsString(), true };
   pushFill(env);
}

void QtGuiAdapter::onNeedStartLogin(const std::string& login)
{
   BsServerMessage msg;
   msg.set_start_login(login);
   Envelope env{ 0, user_, userBS_, {}, {}, msg.SerializeAsString(), true };
   pushFill(env);
}

void QtGuiAdapter::onNeedCancelLogin()
{
   BsServerMessage msg;
   msg.mutable_cancel_last_login();
   Envelope env{ 0, user_, userBS_, {}, {}, msg.SerializeAsString(), true };
   pushFill(env);
}

void QtGuiAdapter::onBootstrapDataLoaded(const std::string& bsData)
{
   SettingsMessage msg;
   msg.set_load_bootstrap(bsData);
   Envelope env{ 0, user_, userSettings_, {}, {}, msg.SerializeAsString(), true };
   pushFill(env);
}

void QtGuiAdapter::onNeedMatchingLogin(const std::string& mtchLogin, const std::string& bsLogin)
{
   MatchingMessage msg;
   auto msgReq = msg.mutable_login();
   msgReq->set_matching_login(mtchLogin);
   msgReq->set_terminal_login(bsLogin);
   Envelope env{ 0, user_, userMatch_, {}, {}, msg.SerializeAsString(), true };
   pushFill(env);
}

void QtGuiAdapter::onNeedMatchingLogout()
{
   MatchingMessage msg;
   msg.mutable_logout();
   Envelope env{ 0, user_, userMatch_, {}, {}, msg.SerializeAsString(), true };
   pushFill(env);
}

void QtGuiAdapter::onNeedSetUserId(const std::string& userId)
{
   WalletsMessage msg;
   msg.set_set_user_id(userId);
   Envelope env{ 0, user_, userWallets_, {}, {}, msg.SerializeAsString(), true };
   pushFill(env);
}

void QtGuiAdapter::onSetRecommendedFeeRate(float fee)
{
   WalletsMessage msg;
   msg.set_set_settlement_fee(fee);
   Envelope env{ 0, user_, userWallets_, {}, {}, msg.SerializeAsString(), true };
   pushFill(env);
}

void QtGuiAdapter::onNeedMdConnection(ApplicationSettings::EnvConfiguration ec)
{
   MktDataMessage msg;
   msg.set_start_connection((int)ec);
   Envelope env{ 0, user_, userMD_, {}, {}, msg.SerializeAsString(), true };
   pushFill(env);
}

void QtGuiAdapter::onNeedNewAuthAddress()
{
   //TODO
}

void QtGuiAdapter::onNeedSubmitAuthAddress(const bs::Address& addr)
{
   BsServerMessage msg;
   msg.set_submit_auth_address(addr.display());
   Envelope env{ 0, user_, userBS_, {}, {}, msg.SerializeAsString(), true };
   pushFill(env);
}

void QtGuiAdapter::onNeedSubmitRFQ(const bs::network::RFQ& rfq, const std::string& reserveId)
{
   SettlementMessage msg;
   auto msgReq = msg.mutable_send_rfq();
   toMsg(rfq, msgReq->mutable_rfq());
   msgReq->set_reserve_id(reserveId);
   Envelope env{ 0, user_, userSettl_, {}, {}, msg.SerializeAsString(), true };
   pushFill(env);
}

void QtGuiAdapter::onNeedAcceptRFQ(const std::string& id, const bs::network::Quote& quote)
{
   SettlementMessage msg;
   auto msgReq = msg.mutable_accept_rfq();
   msgReq->set_rfq_id(id);
   toMsg(quote, msgReq->mutable_quote());
   Envelope env{ 0, user_, userSettl_, {}, {}, msg.SerializeAsString(), true };
   pushFill(env);
}

void QtGuiAdapter::onNeedCancelRFQ(const std::string& id)
{
}

void QtGuiAdapter::onNeedAuthKey(const bs::Address& addr)
{
   WalletsMessage msg;
   msg.set_get_auth_key(addr.display());
   Envelope env{ 0, user_, userWallets_, {}, {}, msg.SerializeAsString(), true };
   pushFill(env);
}

void QtGuiAdapter::onNeedReserveUTXOs(const std::string& reserveId
   , const std::string& subId, uint64_t amount, bool partial
   , const std::vector<UTXO>& utxos)
{
   logger_->debug("[{}] {}/{} {}", __func__, reserveId, subId, amount);
   WalletsMessage msg;
   auto msgReq = msg.mutable_reserve_utxos();
   msgReq->set_id(reserveId);
   msgReq->set_sub_id(subId);
   msgReq->set_amount(amount);
   msgReq->set_partial(partial);
   for (const auto& utxo : utxos) {
      msgReq->add_utxos(utxo.serialize().toBinStr());
   }
   Envelope env{ 0, user_, userWallets_, {}, {}, msg.SerializeAsString(), true };
   pushFill(env);
}

void QtGuiAdapter::onNeedUnreserveUTXOs(const std::string& reserveId
   , const std::string& subId)
{
   WalletsMessage msg;
   auto msgReq = msg.mutable_unreserve_utxos();
   msgReq->set_id(reserveId);
   msgReq->set_sub_id(subId);
   Envelope env{ 0, user_, userWallets_, {}, {}, msg.SerializeAsString(), true };
   pushFill(env);
}

void QtGuiAdapter::processWalletLoaded(const bs::sync::WalletInfo &wi)
{
   hdWallets_[*wi.ids.cbegin()] = wi;
   if (mainWindow_) {
      QMetaObject::invokeMethod(mainWindow_, [this, wi] {
         mainWindow_->onHDWallet(wi);
      });
   }
}

bool QtGuiAdapter::processWalletData(uint64_t msgId
   , const WalletsMessage_WalletData& response)
{
   const auto& itWallet = walletGetMap_.find(msgId);
   if (itWallet == walletGetMap_.end()) {
      return true;
   }
   const auto& walletId = itWallet->second;
   const auto& walletData = bs::sync::WalletData::fromCommonMessage(response);
   if (QMetaObject::invokeMethod(mainWindow_, [this, walletId, walletData] {
      mainWindow_->onWalletData(walletId, walletData);
   })) {
      walletGetMap_.erase(itWallet);
      return true;
   }
   return false;
}

bool QtGuiAdapter::processWalletBalances(const bs::message::Envelope &env
   , const WalletsMessage_WalletBalances &response)
{
   bs::sync::WalletBalanceData wbd;
   wbd.id = response.wallet_id();
   wbd.balTotal = response.total_balance();
   wbd.balSpendable = response.spendable_balance();
   wbd.balUnconfirmed = response.unconfirmed_balance();
   wbd.nbAddresses = response.nb_addresses();
   for (const auto &addrBal : response.address_balances()) {
      wbd.addrBalances.push_back({ BinaryData::fromString(addrBal.address())
         , addrBal.txn(), addrBal.total_balance(), addrBal.spendable_balance()
         , addrBal.unconfirmed_balance() });
   }
   return QMetaObject::invokeMethod(mainWindow_, [this, wbd] {
      mainWindow_->onWalletBalance(wbd);
   });
}

bool QtGuiAdapter::processTXDetails(uint64_t msgId, const WalletsMessage_TXDetailsResponse &response)
{
   std::vector<bs::sync::TXWalletDetails> txDetails;
   for (const auto &resp : response.responses()) {
      bs::sync::TXWalletDetails txDet{ BinaryData::fromString(resp.tx_hash()), resp.wallet_id()
         , resp.wallet_name(), static_cast<bs::core::wallet::Type>(resp.wallet_type())
         , resp.wallet_symbol(), static_cast<bs::sync::Transaction::Direction>(resp.direction())
         , resp.comment(), resp.valid(), resp.amount() };

      const auto &ownTxHash = BinaryData::fromString(resp.tx_hash());
      try {
         if (!resp.tx().empty()) {
            Tx tx(BinaryData::fromString(resp.tx()));
            if (tx.isInitialized()) {
               txDet.tx = std::move(tx);
            }
         }
      } catch (const std::exception &e) {
         logger_->warn("[QtGuiAdapter::processTXDetails] TX deser error: {}", e.what());
      }
      for (const auto &addrStr : resp.out_addresses()) {
         try {
            txDet.outAddresses.push_back(std::move(bs::Address::fromAddressString(addrStr)));
         } catch (const std::exception &e) {
            logger_->warn("[QtGuiAdapter::processTXDetails] out deser error: {}", e.what());
         }
      }
      for (const auto &inAddr : resp.input_addresses()) {
         try {
            txDet.inputAddresses.push_back({ bs::Address::fromAddressString(inAddr.address())
               , inAddr.value(), inAddr.value_string(), inAddr.wallet_name()
               , static_cast<TXOUT_SCRIPT_TYPE>(inAddr.script_type())
               , BinaryData::fromString(inAddr.out_hash()), inAddr.out_index() });
         } catch (const std::exception &e) {
            logger_->warn("[QtGuiAdapter::processTXDetails] input deser error: {}", e.what());
         }
      }
      for (const auto &outAddr : resp.output_addresses()) {
         try {
            txDet.outputAddresses.push_back({ bs::Address::fromAddressString(outAddr.address())
               , outAddr.value(), outAddr.value_string(), outAddr.wallet_name()
               , static_cast<TXOUT_SCRIPT_TYPE>(outAddr.script_type())
               , BinaryData::fromString(outAddr.out_hash()), outAddr.out_index() });
         } catch (const std::exception &e) { // OP_RETURN data for valueStr
            txDet.outputAddresses.push_back({ bs::Address{}
               , outAddr.value(), outAddr.address(), outAddr.wallet_name()
               , static_cast<TXOUT_SCRIPT_TYPE>(outAddr.script_type()), ownTxHash
               , outAddr.out_index() });
         }
      }
      try {
         txDet.changeAddress = { bs::Address::fromAddressString(resp.change_address().address())
            , resp.change_address().value(), resp.change_address().value_string()
            , resp.change_address().wallet_name()
            , static_cast<TXOUT_SCRIPT_TYPE>(resp.change_address().script_type())
            , BinaryData::fromString(resp.change_address().out_hash())
            , resp.change_address().out_index() };
      }
      catch (const std::exception &) {}
      txDetails.push_back(txDet);
   }
   const auto& itZC = newZCs_.find(msgId);
   if (itZC != newZCs_.end()) {
      newZCs_.erase(itZC);
      return QMetaObject::invokeMethod(mainWindow_, [this, txDetails] {
         mainWindow_->onNewZCs(txDetails);
      });
   }
   return QMetaObject::invokeMethod(mainWindow_, [this, txDetails] {
      mainWindow_->onTXDetails(txDetails);
   });
}

bool QtGuiAdapter::processLedgerEntries(const ArmoryMessage_LedgerEntries &response)
{
   std::vector<bs::TXEntry> entries;
   for (const auto &entry : response.entries()) {
      bs::TXEntry txEntry;
      txEntry.txHash = BinaryData::fromString(entry.tx_hash());
      txEntry.value = entry.value();
      txEntry.blockNum = entry.block_num();
      txEntry.txTime = entry.tx_time();
      txEntry.isRBF = entry.rbf();
      txEntry.isChainedZC = entry.chained_zc();
      txEntry.nbConf = entry.nb_conf();
      for (const auto &walletId : entry.wallet_ids()) {
         txEntry.walletIds.insert(walletId);
      }
      for (const auto &addrStr : entry.addresses()) {
         try {
            const auto &addr = bs::Address::fromAddressString(addrStr);
            txEntry.addresses.push_back(addr);
         }
         catch (const std::exception &) {}
      }
      entries.push_back(std::move(txEntry));
   }
   return QMetaObject::invokeMethod(mainWindow_, [this, entries, filter=response.filter()
      , totPages=response.total_pages(), curPage=response.cur_page()
      , curBlock=response.cur_block()] {
         mainWindow_->onLedgerEntries(filter, totPages, curPage, curBlock, entries);
   });
}


bool QtGuiAdapter::processAddressHist(const ArmoryMessage_AddressHistory& response)
{
   bs::Address addr;
   try {
      addr = std::move(bs::Address::fromAddressString(response.address()));
   }
   catch (const std::exception& e) {
      logger_->error("[{}] invalid address: {}", __func__, e.what());
      return true;
   }
   std::vector<bs::TXEntry> entries;
   for (const auto& entry : response.entries()) {
      bs::TXEntry txEntry;
      txEntry.txHash = BinaryData::fromString(entry.tx_hash());
      txEntry.value = entry.value();
      txEntry.blockNum = entry.block_num();
      txEntry.txTime = entry.tx_time();
      txEntry.isRBF = entry.rbf();
      txEntry.isChainedZC = entry.chained_zc();
      txEntry.nbConf = entry.nb_conf();
      for (const auto& walletId : entry.wallet_ids()) {
         txEntry.walletIds.insert(walletId);
      }
      for (const auto& addrStr : entry.addresses()) {
         try {
            const auto& addr = bs::Address::fromAddressString(addrStr);
            txEntry.addresses.push_back(addr);
         }
         catch (const std::exception&) {}
      }
      entries.push_back(std::move(txEntry));
   }
   return QMetaObject::invokeMethod(mainWindow_, [this, entries, addr, curBlock = response.cur_block()] {
         mainWindow_->onAddressHistory(addr, curBlock, entries);
      });
}

bool QtGuiAdapter::processFeeLevels(const ArmoryMessage_FeeLevelsResponse& response)
{
   std::map<unsigned int, float> feeLevels;
   for (const auto& pair : response.fee_levels()) {
      feeLevels[pair.level()] = pair.fee();
   }
   return QMetaObject::invokeMethod(mainWindow_, [this, feeLevels]{
      mainWindow_->onFeeLevels(feeLevels);
   });
}

bool QtGuiAdapter::processWalletsList(const WalletsMessage_WalletsListResponse& response)
{
   std::vector<bs::sync::HDWalletData> wallets;
   for (const auto& wallet : response.wallets()) {
      wallets.push_back(bs::sync::HDWalletData::fromCommonMessage(wallet));
   }

   return QMetaObject::invokeMethod(mainWindow_, [this, wallets, id = response.id()]{
      mainWindow_->onWalletsList(id, wallets);
   });
}

bool QtGuiAdapter::processUTXOs(const WalletsMessage_UtxoListResponse& response)
{
   std::vector<UTXO> utxos;
   for (const auto& serUtxo : response.utxos()) {
      UTXO utxo;
      utxo.unserialize(BinaryData::fromString(serUtxo));
      utxos.push_back(std::move(utxo));
   }
   return QMetaObject::invokeMethod(mainWindow_, [this, utxos, response]{
      mainWindow_->onUTXOs(response.id(), response.wallet_id(), utxos);
   });
}

bool QtGuiAdapter::processSignTX(const BlockSettle::Common::SignerMessage_SignTxResponse& response)
{
   return QMetaObject::invokeMethod(mainWindow_, [this, response] {
      mainWindow_->onSignedTX(response.id(), BinaryData::fromString(response.signed_tx())
         , static_cast<bs::error::ErrorCode>(response.error_code()));
   });
}

bool QtGuiAdapter::processZC(const BlockSettle::Common::ArmoryMessage_ZCReceived& zcs)
{
   WalletsMessage msg;
   auto msgReq = msg.mutable_tx_details_request();
   for (const auto& zcEntry : zcs.tx_entries()) {
      auto txReq = msgReq->add_requests();
      txReq->set_tx_hash(zcEntry.tx_hash());
      if (zcEntry.wallet_ids_size() > 0) {
         txReq->set_wallet_id(zcEntry.wallet_ids(0));
      }
      txReq->set_value(zcEntry.value());
   }
   Envelope env{ 0, user_, userWallets_, {}, {}, msg.SerializeAsString(), true };
   if (!pushFill(env)) {
      return false;
   }
   newZCs_.insert(env.id);
   return true;
}

bool QtGuiAdapter::processZCInvalidated(const ArmoryMessage_ZCInvalidated& zcInv)
{
   std::vector<BinaryData> txHashes;
   for (const auto& hashStr : zcInv.tx_hashes()) {
      txHashes.push_back(BinaryData::fromString(hashStr));
   }
   return QMetaObject::invokeMethod(mainWindow_, [this, txHashes] {
      mainWindow_->onZCsInvalidated(txHashes);
   });
}

bool QtGuiAdapter::processBsServer(const bs::message::Envelope& env)
{
   BsServerMessage msg;
   if (!msg.ParseFromString(env.message)) {
      logger_->error("[{}] failed to parse msg #{}", __func__, env.id);
      return true;
   }
   switch (msg.data_case()) {
   case BsServerMessage::kStartLoginResult:
      return processStartLogin(msg.start_login_result());
   case BsServerMessage::kLoginResult:
      return processLogin(msg.login_result());
   case BsServerMessage::kOrdersUpdate:
      return processOrdersUpdate(msg.orders_update());
   default: break;
   }
   return true;
}

bool QtGuiAdapter::processStartLogin(const BsServerMessage_StartLoginResult& response)
{
   return QMetaObject::invokeMethod(mainWindow_, [this, response] {
      mainWindow_->onLoginStarted(response.login(), response.success()
         , response.error_text());
   });
}

bool QtGuiAdapter::processLogin(const BsServerMessage_LoginResult& response)
{
   BsClientLoginResult result;
   result.login = response.login();
   result.status = static_cast<AutheIDClient::ErrorType>(response.status());
   result.userType = static_cast<bs::network::UserType>(response.user_type());
   result.errorMsg = response.error_text();
   result.celerLogin = response.celer_login();
   result.chatTokenData = BinaryData::fromString(response.chat_token());
   result.chatTokenSign = BinaryData::fromString(response.chat_token_signature());
   result.bootstrapDataSigned = response.bootstrap_signed_data();
   result.enabled = response.enabled();
   result.feeRatePb = response.fee_rate();
   result.tradeSettings.xbtTier1Limit = response.trade_settings().xbt_tier1_limit();
   result.tradeSettings.xbtPriceBand = response.trade_settings().xbt_price_band();
   result.tradeSettings.authRequiredSettledTrades = response.trade_settings().auth_reqd_settl_trades();
   result.tradeSettings.authSubmitAddressLimit = response.trade_settings().auth_submit_addr_limit();
   result.tradeSettings.dealerAuthSubmitAddressLimit = response.trade_settings().dealer_auth_submit_addr_limit();

   return QMetaObject::invokeMethod(mainWindow_, [this, result] {
      mainWindow_->onLoggedIn(result);
   });
}

bool QtGuiAdapter::processSettlement(const bs::message::Envelope& env)
{
   SettlementMessage msg;
   if (!msg.ParseFromString(env.message)) {
      logger_->error("[{}] failed to parse msg #{}", __func__, env.id);
      return true;
   }
   switch (msg.data_case()) {
   case SettlementMessage::kQuote:
      return processQuote(msg.quote());
   case SettlementMessage::kMatchedQuote:
      return processMatchedQuote(msg.matched_quote());
   case SettlementMessage::kFailedSettlement:
      return processFailedSettl(msg.failed_settlement());
   case SettlementMessage::kPendingSettlement:
      return processPendingSettl(msg.pending_settlement());
   case SettlementMessage::kSettlementComplete:
      return processSettlComplete(msg.settlement_complete());
   default:    break;
   }
   return true;
}

bool QtGuiAdapter::processMatching(const bs::message::Envelope& env)
{
   MatchingMessage msg;
   if (!msg.ParseFromString(env.message)) {
      logger_->error("[{}] failed to parse msg #{}", __func__, env.id);
      return true;
   }
   switch (msg.data_case()) {
   case MatchingMessage::kLoggedIn:
      return QMetaObject::invokeMethod(mainWindow_, [this, response=msg.logged_in()] {
         mainWindow_->onMatchingLogin(response.user_name()
            , static_cast<BaseCelerClient::CelerUserType>(response.user_type()), response.user_id());
      });
   case MatchingMessage::kLoggedOut:
      return QMetaObject::invokeMethod(mainWindow_, [this] {
         mainWindow_->onMatchingLogout();
      });
/*   case MatchingMessage::kQuoteNotif:
      return processQuoteNotif(msg.quote_notif());*/
   default:    break;
   }
   return true;
}

bool QtGuiAdapter::processMktData(const bs::message::Envelope& env)
{
   MktDataMessage msg;
   if (!msg.ParseFromString(env.message)) {
      logger_->error("[{}] failed to parse msg #{}", __func__, env.id);
      return true;
   }
   switch (msg.data_case()) {
   case MktDataMessage::kDisconnected:
      mdInstrumentsReceived_ = false;
      break;
   case MktDataMessage::kNewSecurity:
      return processSecurity(msg.new_security().name(), msg.new_security().asset_type());
   case MktDataMessage::kAllInstrumentsReceived:
      mdInstrumentsReceived_ = true;
      return sendPooledOrdersUpdate();
   case MktDataMessage::kPriceUpdate:
      return processMdUpdate(msg.price_update());
   default: break;
   }
   return true;
}

bool QtGuiAdapter::processSecurity(const std::string& name, int assetType)
{
   assetTypes_[name] = static_cast<bs::network::Asset::Type>(assetType);
   return true;
}

bool QtGuiAdapter::processMdUpdate(const MktDataMessage_Prices& msg)
{
   return QMetaObject::invokeMethod(mainWindow_, [this, msg] {
      const bs::network::MDFields fields{
         { bs::network::MDField::Type::PriceBid, msg.bid() },
         { bs::network::MDField::Type::PriceOffer, msg.ask() },
         { bs::network::MDField::Type::PriceLast, msg.last() },
         { bs::network::MDField::Type::DailyVolume, msg.volume() },
         { bs::network::MDField::Type::MDTimestamp, (double)msg.timestamp() }
      };
      mainWindow_->onMDUpdated(static_cast<bs::network::Asset::Type>(msg.security().asset_type())
         , QString::fromStdString(msg.security().name()), fields);
   });
}

bool QtGuiAdapter::processAuthWallet(const WalletsMessage_WalletData& authWallet)
{
   std::vector<bs::Address> authAddresses;
   OnChainTrackMessage msg;
   auto msgReq = msg.mutable_set_auth_addresses();
   msgReq->set_wallet_id(authWallet.wallet_id());
   for (const auto& addr : authWallet.used_addresses()) {
      msgReq->add_addresses(addr.address());
      try {
         authAddresses.push_back(bs::Address::fromAddressString(addr.address()));
      }
      catch (const std::exception&) {}
   }
   Envelope env{ 0, user_, userTrk_, {}, {}, msg.SerializeAsString(), true };
   pushFill(env);
   return QMetaObject::invokeMethod(mainWindow_, [this, authAddresses] {
      mainWindow_->onAuthAddresses(authAddresses, {});
   });
}

bool QtGuiAdapter::processAuthState(const OnChainTrackMessage_AuthState& authState)
{
   bs::Address addr;
   try {
      addr = bs::Address::fromAddressString(authState.address());
   }
   catch (const std::exception&) {
      return true;
   }
   const auto& state = static_cast<AddressVerificationState>(authState.state());
   return QMetaObject::invokeMethod(mainWindow_, [this, addr, state] {
      mainWindow_->onAuthAddresses({},  { {addr, state } });
   });
}

bool QtGuiAdapter::processSubmittedAuthAddrs(const AssetsMessage_SubmittedAuthAddresses& addrs)
{
   std::vector<bs::Address> authAddresses;
   for (const auto& addr : addrs.addresses()) {
      try {
         authAddresses.push_back(bs::Address::fromAddressString(addr));
      }
      catch (const std::exception&) {}
   }
   return QMetaObject::invokeMethod(mainWindow_, [this, authAddresses] {
      mainWindow_->onSubmittedAuthAddresses(authAddresses);
   });
}

bool QtGuiAdapter::processBalance(const AssetsMessage_Balance& bal)
{
   return QMetaObject::invokeMethod(mainWindow_, [this, bal] {
      mainWindow_->onBalance(bal.currency(), bal.value());
   });
}

bool QtGuiAdapter::processVerifiedAuthAddrs(const OnChainTrackMessage_AuthAddresses& addrs)
{
   std::vector<bs::Address> authAddresses;
   for (const auto& addr : addrs.addresses()) {
      try {
         authAddresses.push_back(bs::Address::fromAddressString(addr));
      } catch (const std::exception&) {}
   }
   return QMetaObject::invokeMethod(mainWindow_, [this, authAddresses] {
      mainWindow_->onVerifiedAuthAddresses(authAddresses);
   });
}

bool QtGuiAdapter::processAuthKey(const WalletsMessage_AuthKey& response)
{
   return QMetaObject::invokeMethod(mainWindow_, [this, response] {
      try {
         mainWindow_->onAuthKey(bs::Address::fromAddressString(response.auth_address())
            , BinaryData::fromString(response.auth_key()));
      }
      catch (const std::exception&) {}
   });
}

bool QtGuiAdapter::processQuote(const BlockSettle::Terminal::Quote& msg)
{
   const auto& quote = fromMsg(msg);
   return QMetaObject::invokeMethod(mainWindow_, [this, quote] {
      mainWindow_->onQuoteReceived(quote);
   });
}

bool QtGuiAdapter::processMatchedQuote(const SettlementMessage_MatchedQuote& msg)
{
   return QMetaObject::invokeMethod(mainWindow_, [this, msg] {
      mainWindow_->onQuoteMatched(msg.rfq_id(), msg.quote_id());
   });
}

bool QtGuiAdapter::processFailedSettl(const SettlementMessage_FailedSettlement& msg)
{
   return QMetaObject::invokeMethod(mainWindow_, [this, msg] {
      if (!msg.quote_id().empty() && !msg.rfq_id().empty()) {
         mainWindow_->onQuoteFailed(msg.rfq_id(), msg.quote_id(), msg.info());
      }
      else {
         //TODO: another type of failure
      }
   });
}

bool QtGuiAdapter::processPendingSettl(const SettlementMessage_SettlementIds& msg)
{
   return QMetaObject::invokeMethod(mainWindow_, [this, msg] {
      mainWindow_->onSettlementPending(msg.rfq_id(), msg.quote_id()
         , BinaryData::fromString(msg.settlement_id()));
   });
}

bool QtGuiAdapter::processSettlComplete(const SettlementMessage_SettlementIds& msg)
{
   return QMetaObject::invokeMethod(mainWindow_, [this, msg] {
      mainWindow_->onSettlementComplete(msg.rfq_id(), msg.quote_id()
         , BinaryData::fromString(msg.settlement_id()));
   });
}

bool QtGuiAdapter::processOrdersUpdate(const BlockSettle::Terminal::BsServerMessage_Orders& msg)
{
   // Use some fake orderId so old code works correctly
   int orderId = 0;
   std::vector<bs::network::Order> orders;
   for (const auto& o : msg.orders()) {
      bs::network::Order order;
      order.security = o.product() + "/" + o.contra_product();
      auto itAsset = assetTypes_.find(order.security);
      if (itAsset != assetTypes_.end()) {
         order.assetType = itAsset->second;
      }
      else {
         order.assetType = bs::network::Asset::Undefined;
      }
      order.exchOrderId = QString::number(++orderId);
      order.status = static_cast<bs::network::Order::Status>(o.status());
      order.side = o.buy() ? bs::network::Side::Buy : bs::network::Side::Sell;
      order.pendingStatus = o.status_text();
      order.dateTime = QDateTime::fromMSecsSinceEpoch(o.timestamp());
      order.product = o.product();
      order.quantity = o.quantity();
      order.price = o.price();
      orders.push_back(order);
   }
   pooledOrders_ = std::move(orders);
   return sendPooledOrdersUpdate();
}

bool QtGuiAdapter::sendPooledOrdersUpdate()
{
   if (!mdInstrumentsReceived_ || pooledOrders_.empty()) {
      return true;
   }
   for (auto& order : pooledOrders_) {
      if (order.assetType == bs::network::Asset::Undefined) {
         const auto& itAsset = assetTypes_.find(order.security);
         if (itAsset != assetTypes_.end()) {
            order.assetType = itAsset->second;
         }
      }
   }
   return QMetaObject::invokeMethod(mainWindow_, [this] {
      mainWindow_->onOrdersUpdate(pooledOrders_);
      pooledOrders_.clear();
   });
}

bool QtGuiAdapter::processReservedUTXOs(const WalletsMessage_ReservedUTXOs& response)
{
   std::vector<UTXO> utxos;
   for (const auto& utxoSer : response.utxos()) {
      UTXO utxo;
      utxo.unserialize(BinaryData::fromString(utxoSer));
      utxos.push_back(std::move(utxo));
   }
   return QMetaObject::invokeMethod(mainWindow_, [this, response, utxos] {
      mainWindow_->onReservedUTXOs(response.id(), response.sub_id(), utxos);
   });
}

#include "QtGuiAdapter.moc"
