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
   , userBlockchain_(std::make_shared<UserTerminal>(TerminalUsers::Blockchain))
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

      default: {
         int idx = setting.request().index();
         QVariant value;
         switch (setting.request().type()) {
         case SettingType_String:
            value = QString::fromStdString(setting.s());
            break;
         case SettingType_Int:
            value = setting.i();
            break;
         case SettingType_UInt:
            value = setting.ui();
            break;
         case SettingType_UInt64:
            value = setting.ui64();
            break;
         case SettingType_Bool:
            value = setting.b();
            break;
         case SettingType_Float:
            value = setting.f();
            break;
         case SettingType_Rect:
            value = QRect(setting.rect().left(), setting.rect().top()
               , setting.rect().width(), setting.rect().height());
            break;
         case SettingType_Strings: {
            QStringList sl;
            for (const auto &s : setting.strings().strings()) {
               sl << QString::fromStdString(s);
            }
            value = sl;
         }
            break;
         case SettingType_StrMap: {
            QVariantMap vm;
            for (const auto &keyVal : setting.key_vals().key_vals()) {
               vm[QString::fromStdString(keyVal.key())] = QString::fromStdString(keyVal.value());
            }
            value = vm;
         }
            break;
         default: break;
         }
         QMetaObject::invokeMethod(mainWindow_, [mw = mainWindow_, idx, value] {
            mw->onSetting(idx, value);
         });
      }
         break;
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
      return processLedgerEntries(env, msg.ledger_entries());
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

   case WalletsMessage::kWalletAddresses: {
      std::vector<bs::sync::Address> addresses;
      addresses.reserve(msg.wallet_addresses().addresses_size());
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

   case WalletsMessage::kWalletBalances:
      return processWalletBalances(env, msg.wallet_balances());
   case WalletsMessage::kTxDetailsResponse:
      return processTXDetails(msg.tx_details_response());
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

   Envelope env{ 0, user_, userSettings_, {}, {}, msg.SerializeAsString(), true };
   pushFill(env);
}

void QtGuiAdapter::makeMainWinConnections()
{
   connect(mainWindow_, &bs::gui::qt::MainWindow::putSetting, this, &QtGuiAdapter::onPutSetting);
   connect(mainWindow_, &bs::gui::qt::MainWindow::needHDWalletDetails, this, &QtGuiAdapter::onNeedHDWalletDetails);
   connect(mainWindow_, &bs::gui::qt::MainWindow::needWalletBalances, this, &QtGuiAdapter::onNeedWalletBalances);
   connect(mainWindow_, &bs::gui::qt::MainWindow::needSpendableUTXOs, this, &QtGuiAdapter::onNeedSpendableUTXOs);
   connect(mainWindow_, &bs::gui::qt::MainWindow::needExtAddresses, this, &QtGuiAdapter::onNeedExtAddresses);
   connect(mainWindow_, &bs::gui::qt::MainWindow::needIntAddresses, this, &QtGuiAdapter::onNeedIntAddresses);
   connect(mainWindow_, &bs::gui::qt::MainWindow::needUsedAddresses, this, &QtGuiAdapter::onNeedUsedAddresses);
   connect(mainWindow_, &bs::gui::qt::MainWindow::needAddrComments, this, &QtGuiAdapter::onNeedAddrComments);
   connect(mainWindow_, &bs::gui::qt::MainWindow::setAddrComment, this, &QtGuiAdapter::onSetAddrComment);
   connect(mainWindow_, &bs::gui::qt::MainWindow::needLedgerEntries, this, &QtGuiAdapter::onNeedLedgerEntries);
   connect(mainWindow_, &bs::gui::qt::MainWindow::needTXDetails, this, &QtGuiAdapter::onNeedTXDetails);
}

void QtGuiAdapter::onPutSetting(int idx, const QVariant &value)
{
   SettingsMessage msg;
   auto msgReq = msg.mutable_put_request();
   auto setResp = msgReq->add_responses();
   auto setReq = setResp->mutable_request();
   setReq->set_source(SettingSource_Local);
   setReq->set_index(static_cast<SettingIndex>(idx));
   switch (value.type()) {
   case QVariant::Type::String:
      setReq->set_type(SettingType_String);
      setResp->set_s(value.toString().toStdString());
      break;
   case QVariant::Type::Int:
      setReq->set_type(SettingType_Int);
      setResp->set_i(value.toInt());
      break;
   case QVariant::Type::UInt:
      setReq->set_type(SettingType_UInt);
      setResp->set_ui(value.toUInt());
      break;
   case QVariant::Type::ULongLong:
   case QVariant::Type::LongLong:
      setReq->set_type(SettingType_UInt64);
      setResp->set_ui64(value.toULongLong());
      break;
   case QVariant::Type::Double:
      setReq->set_type(SettingType_Float);
      setResp->set_f(value.toDouble());
      break;
   case QVariant::Type::Bool:
      setReq->set_type(SettingType_Bool);
      setResp->set_b(value.toBool());
      break;
   case QVariant::Type::Rect:
      setReq->set_type(SettingType_Rect);
      {
         auto setRect = setResp->mutable_rect();
         setRect->set_left(value.toRect().left());
         setRect->set_top(value.toRect().top());
         setRect->set_height(value.toRect().height());
         setRect->set_width(value.toRect().width());
      }
      break;
   case QVariant::Type::StringList:
      setReq->set_type(SettingType_Strings);
      for (const auto &s : value.toStringList()) {
         setResp->mutable_strings()->add_strings(s.toStdString());
      }
      break;
   case QVariant::Type::Map:
      setReq->set_type(SettingType_StrMap);
      for (const auto &key : value.toMap().keys()) {
         auto kvData = setResp->mutable_key_vals()->add_key_vals();
         kvData->set_key(key.toStdString());
         kvData->set_value(value.toMap()[key].toString().toStdString());
      }
      break;
   }

   Envelope env{ 0, user_, userSettings_, {}, {}, msg.SerializeAsString(), true };
   pushFill(env);
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

void QtGuiAdapter::onNeedWalletBalances(const std::string &walletId)
{
   WalletsMessage msg;
   msg.set_get_wallet_balances(walletId);
   Envelope env{ 0, user_, userWallets_, {}, {}, msg.SerializeAsString(), true };
   pushFill(env);
}

void QtGuiAdapter::onNeedSpendableUTXOs(const std::string &walletId)
{
   WalletsMessage msg;
   msg.set_get_spendable_utxos(walletId);
   Envelope env{ 0, user_, userWallets_, {}, {}, msg.SerializeAsString(), true };
   pushFill(env);
}

void QtGuiAdapter::onNeedExtAddresses(const std::string &walletId)
{
   WalletsMessage msg;
   msg.set_get_ext_addresses(walletId);
   Envelope env{ 0, user_, userWallets_, {}, {}, msg.SerializeAsString(), true };
   pushFill(env);
}

void QtGuiAdapter::onNeedIntAddresses(const std::string &walletId)
{
   WalletsMessage msg;
   msg.set_get_ext_addresses(walletId);
   Envelope env{ 0, user_, userWallets_, {}, {}, msg.SerializeAsString(), true };
   pushFill(env);
}

void QtGuiAdapter::onNeedUsedAddresses(const std::string &walletId)
{
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

void QtGuiAdapter::onNeedTXDetails(const const std::vector<bs::sync::TXWallet> &txWallet)
{
   WalletsMessage msg;
   auto msgReq = msg.mutable_tx_details_request();
   for (const auto &txw : txWallet) {
      auto request = msgReq->add_requests();
      request->set_tx_hash(txw.txHash.toBinStr());
      request->set_wallet_id(txw.walletId);
      request->set_value(txw.value);
   }
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
   QMetaObject::invokeMethod(mainWindow_, [this, wbd] {
      mainWindow_->onWalletBalance(wbd);
   });
   return true;
}

bool QtGuiAdapter::processTXDetails(const WalletsMessage_TXDetailsResponse &response)
{
   std::vector<bs::sync::TXWalletDetails> txDetails;
   for (const auto &resp : response.responses()) {
      bs::sync::TXWalletDetails txDet{ BinaryData::fromString(resp.tx_hash()), resp.wallet_id()
         , resp.wallet_name(), static_cast<bs::core::wallet::Type>(resp.wallet_type())
         , static_cast<bs::sync::Transaction::Direction>(resp.direction())
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
            txDet.outAddresses.emplace_back(std::move(bs::Address::fromAddressString(addrStr)));
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
               , static_cast<TXOUT_SCRIPT_TYPE>(outAddr.script_type()), ownTxHash
               , outAddr.out_index() });
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
            , ownTxHash, resp.change_address().out_index() };
      }
      catch (const std::exception &) {}
      txDetails.push_back(txDet);
   }
   QMetaObject::invokeMethod(mainWindow_, [this, txDetails] {
      mainWindow_->onTXDetails(txDetails);
   });
   return true;
}

bool QtGuiAdapter::processLedgerEntries(const bs::message::Envelope &env
   , const ArmoryMessage_LedgerEntries &response)
{
   std::vector<bs::TXEntry> entries;
   entries.reserve(response.entries_size());
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
            txEntry.addresses.emplace_back(std::move(addr));
         }
         catch (const std::exception &) {}
      }
      entries.emplace_back(std::move(txEntry));
   }
   QMetaObject::invokeMethod(mainWindow_, [this, entries, filter=response.filter()
      , totPages=response.total_pages(), curPage=response.cur_page()
      , curBlock=response.cur_block()] {
         mainWindow_->onLedgerEntries(filter, totPages, curPage, curBlock, entries);
   });
   return true;
}


#include "QtGuiAdapter.moc"
