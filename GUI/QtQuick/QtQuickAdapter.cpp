/*

***********************************************************************************
* Copyright (C) 2020 - 2022, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "CommonTypes.h"
#include "QtQuickAdapter.h"
#include <QApplication>
#include <QDateTime>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QIcon>
#include <QLockFile>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QPalette>
#include <QStandardPaths>
#include <QThread>
#include <QTimer>
#include <spdlog/spdlog.h>
#include "BSMessageBox.h"
#include "BSTerminalSplashScreen.h"
#include "Wallets/ProtobufHeadlessUtils.h"
#include "SettingsAdapter.h"

#include "common.pb.h"
#include "terminal.pb.h"

using namespace BlockSettle::Common;
using namespace BlockSettle::Terminal;
using namespace bs::message;


namespace {
   std::shared_ptr<spdlog::logger> staticLogger;
}
// redirect qDebug() to the log
// stdout redirected to parent process
void qMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
   QByteArray localMsg = msg.toLocal8Bit();
   switch (type) {
   case QtDebugMsg:
      staticLogger->debug("[QML] {}", localMsg.constData());
      break;
   case QtInfoMsg:
      staticLogger->info("[QML] {}", localMsg.constData());
      break;
   case QtWarningMsg:
      staticLogger->warn("[QML] {}", localMsg.constData());
      break;
   case QtCriticalMsg:
      staticLogger->error("[QML] {}", localMsg.constData());
      break;
   case QtFatalMsg:
      staticLogger->critical("[QML] {}", localMsg.constData());
      break;
   }
}

static void checkStyleSheet(QApplication& app)
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

QtQuickAdapter::QtQuickAdapter(const std::shared_ptr<spdlog::logger> &logger)
   : QObject(nullptr), logger_(logger)
   , userSettings_(std::make_shared<UserTerminal>(TerminalUsers::Settings))
   , userWallets_(std::make_shared<UserTerminal>(TerminalUsers::Wallets))
   , userBlockchain_(std::make_shared<UserTerminal>(TerminalUsers::Blockchain))
   , userSigner_(std::make_shared<UserTerminal>(TerminalUsers::Signer))
{
   staticLogger = logger;
}

QtQuickAdapter::~QtQuickAdapter()
{}

void QtQuickAdapter::run(int &argc, char **argv)
{
   logger_->debug("[QtQuickAdapter::run]");
   Q_INIT_RESOURCE(armory);
   Q_INIT_RESOURCE(qtquick);

//   QGuiApplication::setAttribute(Qt::AA_EnableHighDpiScaling);

   QApplication app(argc, argv);

   QApplication::setOrganizationDomain(QLatin1String("blocksettle.com"));
   QApplication::setWindowIcon(QIcon(QStringLiteral(":/images/bs_logo.png")));

   const QFileInfo localStyleSheetFile(QLatin1String("stylesheet.css"));
   QFile stylesheetFile(localStyleSheetFile.exists()
      ? localStyleSheetFile.fileName() : QLatin1String(":/STYLESHEET"));

   if (stylesheetFile.open(QFile::ReadOnly)) {
      app.setStyleSheet(QString::fromLatin1(stylesheetFile.readAll()));
      QPalette p = QApplication::palette();
      p.setColor(QPalette::Disabled, QPalette::Light, QColor(10, 22, 25));
      QApplication::setPalette(p);
   }

   // Start monitoring to update stylesheet live when file is changed on the disk
   QTimer timer;
   QObject::connect(&timer, &QTimer::timeout, &app, [&app] {
      checkStyleSheet(app);
   });
   timer.start(100);

   const QString location = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
   const QString lockFilePath = location + QLatin1String("/blocksettle.lock");
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

   qInstallMessageHandler(qMessageHandler);

   QString logoIcon;
   logoIcon = QLatin1String(":/FULL_BS_LOGO");

   QPixmap splashLogo(logoIcon);
   const int splashScreenWidth = 400;
   {
      std::lock_guard<std::recursive_mutex> lock(mutex_);
      splashScreen_ = new BSTerminalSplashScreen(splashLogo.scaledToWidth(splashScreenWidth
         , Qt::SmoothTransformation));
   }
   updateSplashProgress();
   splashScreen_->show();
   QMetaObject::invokeMethod(splashScreen_, [this] {
      QTimer::singleShot(5000, [this] {
         splashProgressCompleted();
      });
   });

   logger_->debug("[QtGuiAdapter::run] creating QML app");
   QQmlApplicationEngine engine;
   QQuickWindow::setTextRenderType(QQuickWindow::NativeTextRendering);
   const QFont fixedFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
   engine.rootContext()->setContextProperty(QStringLiteral("fixedFont"), fixedFont);
   engine.load(QUrl(QStringLiteral("qrc:/qml/main.qml")));
   if (engine.rootObjects().empty()) {
      BSMessageBox box(BSMessageBox::critical, app.tr("BlockSettle Terminal")
         , app.tr("Failed to load QML GUI"), app.tr("See log for details"));
      box.exec();
      return;
   }
   rootObj_ = engine.rootObjects().at(0);
   if (loadingDone_) {
      auto window = qobject_cast<QQuickWindow*>(rootObj_);
      if (window) {
         window->show();
      }
   }

   updateStates();

   requestInitialSettings();
   logger_->debug("[QtGuiAdapter::run] initial setup done");
   app.exec();
}

bool QtQuickAdapter::process(const Envelope &env)
{
   if (std::dynamic_pointer_cast<UserTerminal>(env.sender)) {
      switch (env.sender->value<bs::message::TerminalUsers>()) {
      case TerminalUsers::Settings:
         return processSettings(env);
      case TerminalUsers::Blockchain:
         return processBlockchain(env);
      case TerminalUsers::Signer:
         return processSigner(env);
      case TerminalUsers::Wallets:
         return processWallets(env);
      default:    break;
      }
   }
   return true;
}

bool QtQuickAdapter::processBroadcast(const bs::message::Envelope& env)
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
      default:    break;
      }
   }
   return false;
}

bool QtQuickAdapter::processSettings(const Envelope &env)
{
   SettingsMessage msg;
   if (!msg.ParseFromString(env.message)) {
      logger_->error("[{}] failed to parse settings msg #{}", __func__, env.foreignId());
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
   default: break;
   }
   return true;
}

bool QtQuickAdapter::processSettingsGetResponse(const SettingsMessage_SettingsResponse &response)
{
   std::map<int, QVariant> settings;
   for (const auto &setting : response.responses()) {
      switch (setting.request().index()) {
      case SetIdx_GUI_MainGeom: {
         QRect mainGeometry(setting.rect().left(), setting.rect().top()
            , setting.rect().width(), setting.rect().height());
         if (splashScreen_) {
//            const auto &currentDisplay = getDisplay(mainGeometry.center());
//            auto splashGeometry = splashScreen_->geometry();
//            splashGeometry.moveCenter(currentDisplay->geometry().center());
//            QMetaObject::invokeMethod(splashScreen_, [ss=splashScreen_, splashGeometry] {
//               ss->setGeometry(splashGeometry);
//            });
         }
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
            //onResetSettings({});
         }
         break;

      default:
         settings[setting.request().index()] = fromResponse(setting);
         break;
      }
   }
   if (!settings.empty()) {
      //TODO: propagate settings to GUI
   }
   return true;
}

bool QtQuickAdapter::processSettingsState(const SettingsMessage_SettingsResponse& response)
{
   ApplicationSettings::State state;
   for (const auto& setting : response.responses()) {
      state[static_cast<ApplicationSettings::Setting>(setting.request().index())] =
         fromResponse(setting);
   }
   //TODO: process setting
   return true;
}

bool QtQuickAdapter::processArmoryServers(const SettingsMessage_ArmoryServers& response)
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
   //TODO
   return true;
}

bool QtQuickAdapter::processAdminMessage(const Envelope &env)
{
   AdministrativeMessage msg;
   if (!msg.ParseFromString(env.message)) {
      logger_->error("[{}] failed to parse admin msg #{}", __func__, env.foreignId());
      return true;
   }
   switch (msg.data_case()) {
   case AdministrativeMessage::kComponentCreated:
      switch (static_cast<TerminalUsers>(msg.component_created())) {
      case TerminalUsers::Unknown:
      case TerminalUsers::API:
      case TerminalUsers::Settings:
         break;
      default:
         createdComponents_.insert(msg.component_created());
         break;
      }
      break;
   case AdministrativeMessage::kComponentLoading: {
      std::lock_guard<std::recursive_mutex> lock(mutex_);
      loadingComponents_.insert(msg.component_loading());
      break;
   }
   default: break;
   }
   updateSplashProgress();
   return true;
}

bool QtQuickAdapter::processBlockchain(const Envelope &env)
{
   ArmoryMessage msg;
   if (!msg.ParseFromString(env.message)) {
      logger_->error("[QtGuiAdapter::processBlockchain] failed to parse msg #{}"
         , env.foreignId());
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
      //TODO
      break;
   case ArmoryMessage::kNewBlock:
      blockNum_ = msg.new_block().top_block();
      //TODO
      break;
   case ArmoryMessage::kWalletRegistered:
      if (msg.wallet_registered().success() && msg.wallet_registered().wallet_id().empty()) {
         walletsReady_ = true;
         //TODO
      }
      break;
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

bool QtQuickAdapter::processSigner(const Envelope &env)
{
   SignerMessage msg;
   if (!msg.ParseFromString(env.message)) {
      logger_->error("[QtGuiAdapter::processSigner] failed to parse msg #{}"
         , env.foreignId());
      if (!env.receiver) {
         logger_->debug("[{}] no receiver", __func__);
      }
      return true;
   }
   switch (msg.data_case()) {
   case SignerMessage::kState:
      signerState_ = msg.state().code();
      signerDetails_ = msg.state().text();
      //TODO
      break;
   case SignerMessage::kNeedNewWalletPrompt:
      createWallet(true);
      break;
   case SignerMessage::kSignTxResponse:
      return processSignTX(msg.sign_tx_response());
   case SignerMessage::kWalletDeleted:
      {
         const auto& itWallet = hdWallets_.find(msg.wallet_deleted());
         bs::sync::WalletInfo wi;
         if (itWallet == hdWallets_.end()) {
            wi.ids.push_back(msg.wallet_deleted());
         } else {
            wi = itWallet->second;
         }
         //TODO
      }
      break;
   default:    break;
   }
   return true;
}

bool QtQuickAdapter::processWallets(const Envelope &env)
{
   WalletsMessage msg;
   if (!msg.ParseFromString(env.message)) {
      logger_->error("[{}] failed to parse msg #{}", __func__, env.foreignId());
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
      //TODO
   }
      break;

   case WalletsMessage::kWalletDeleted: {
      const auto& wi = bs::sync::WalletInfo::fromCommonMsg(msg.wallet_deleted());
      //TODO
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
      const auto& walletId = msg.wallet_addresses().wallet_id();
      auto itReq = needChangeAddrReqs_.find(env.responseId());
      if (itReq != needChangeAddrReqs_.end()) {
         //TODO
         needChangeAddrReqs_.erase(itReq);
      }
      else {
         //TODO
      }
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
      //TODO
   }
      break;
   case WalletsMessage::kWalletData:
      return processWalletData(env.responseId(), msg.wallet_data());
   case WalletsMessage::kWalletBalances:
      return processWalletBalances(env, msg.wallet_balances());
   case WalletsMessage::kTxDetailsResponse:
      return processTXDetails(env.responseId(), msg.tx_details_response());
   case WalletsMessage::kWalletsListResponse:
      return processWalletsList(msg.wallets_list_response());
   case WalletsMessage::kUtxos:
      return processUTXOs(msg.utxos());
   case WalletsMessage::kReservedUtxos:
      return processReservedUTXOs(msg.reserved_utxos());
   case WalletsMessage::kWalletChanged:
      if (walletsReady_) {
         //TODO: onNeedLedgerEntries({});
      }
      break;
   case WalletsMessage::kLedgerEntries:
      return processLedgerEntries(msg.ledger_entries());
   default:    break;
   }
   return true;
}

void QtQuickAdapter::updateStates()
{
   //TODO
}

//#define DEBUG_LOADING_PROGRESS
void QtQuickAdapter::updateSplashProgress()
{
   std::lock_guard<std::recursive_mutex> lock(mutex_);
   if (createdComponents_.empty()) {
      if (splashScreen_) {
         QMetaObject::invokeMethod(splashScreen_, [this] {
            QTimer::singleShot(100, [this] {
               splashScreen_->hide();
               splashScreen_->deleteLater();
               splashScreen_ = nullptr;
            });
         });
      }
      return;
   }
   auto percent = unsigned(100 * loadingComponents_.size() / createdComponents_.size());
#ifdef DEBUG_LOADING_PROGRESS
   std::string l, c;
   for (const auto &lc : loadingComponents_) {
      l += std::to_string(lc) + " ";
   }
   for (const auto &cc : createdComponents_) {
      c += std::to_string(cc) + " ";
   }
   logger_->debug("[{}] {} / {} = {}%", __func__, l, c, percent);
#endif
   if (splashScreen_) {
      QMetaObject::invokeMethod(splashScreen_, [this, percent] {
         splashScreen_->SetProgress(percent);
      });
   }
   if (percent >= 100) {
      splashProgressCompleted();
   }
}

void QtQuickAdapter::splashProgressCompleted()
{
   {
      std::lock_guard<std::recursive_mutex> lock(mutex_);
      loadingDone_ = true;
      loadingComponents_.clear();
      createdComponents_.clear();
   }
   if (!splashScreen_) {
      return;
   }
   QMetaObject::invokeMethod(splashScreen_, [this] {
      auto window = qobject_cast<QQuickWindow*>(rootObj_);
      if (window) {
         window->show();
      }
      else {
         logger_->error("[QtQuickAdapter::splashProgressCompleted] no main window found");
      }
      QTimer::singleShot(100, [this] {
         splashScreen_->hide();
         splashScreen_->deleteLater();
         splashScreen_ = nullptr;
      });
   });
}

void QtQuickAdapter::requestInitialSettings()
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

   pushRequest(user_, userSettings_, msg.SerializeAsString());
}

void QtQuickAdapter::createWallet(bool primary)
{
   logger_->debug("[{}] primary: {}", __func__, primary);
}

void QtQuickAdapter::processWalletLoaded(const bs::sync::WalletInfo &wi)
{
   hdWallets_[*wi.ids.cbegin()] = wi;
   //TODO
}

bool QtQuickAdapter::processWalletData(uint64_t msgId
   , const WalletsMessage_WalletData& response)
{
   const auto& itWallet = walletGetMap_.find(msgId);
   if (itWallet == walletGetMap_.end()) {
      return true;
   }
   const auto& walletId = itWallet->second;
   const auto& walletData = bs::sync::WalletData::fromCommonMessage(response);
   //TODO
   {
      walletGetMap_.erase(itWallet);
      return true;
   }
   return false;
}

bool QtQuickAdapter::processWalletBalances(const bs::message::Envelope &
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
         , addrBal.tx_count(), (int64_t)addrBal.total_balance(), (int64_t)addrBal.spendable_balance()
         , (int64_t)addrBal.unconfirmed_balance() });
   }
   //TODO
   return true;
}

bool QtQuickAdapter::processTXDetails(uint64_t msgId, const WalletsMessage_TXDetailsResponse &response)
{
   std::vector<bs::sync::TXWalletDetails> txDetails;
   for (const auto &resp : response.responses()) {
      bs::sync::TXWalletDetails txDet{ BinaryData::fromString(resp.tx_hash()), resp.wallet_id()
         , resp.wallet_name(), static_cast<bs::core::wallet::Type>(resp.wallet_type())
         , resp.wallet_symbol(), static_cast<bs::sync::Transaction::Direction>(resp.direction())
         , resp.comment(), resp.valid(), resp.amount() };
      if (!response.error_msg().empty()) {
         txDet.comment = response.error_msg();
      }

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
         } catch (const std::exception &) { // OP_RETURN data for valueStr
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
      txDetails.emplace_back(std::move(txDet));
   }
   if (!response.responses_size() && !response.error_msg().empty()) {
      bs::sync::TXWalletDetails txDet;
      txDet.comment = response.error_msg();
      txDetails.emplace_back(std::move(txDet));
   }
   const auto& itZC = newZCs_.find(msgId);
   if (itZC != newZCs_.end()) {
      newZCs_.erase(itZC);
      //TODO
   }
   else {
      //TODO
   }
   return true;
}

bool QtQuickAdapter::processLedgerEntries(const LedgerEntries &response)
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
   //TODO
   return true;
}


bool QtQuickAdapter::processAddressHist(const ArmoryMessage_AddressHistory& response)
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
   //TODO
   return true;
}

bool QtQuickAdapter::processFeeLevels(const ArmoryMessage_FeeLevelsResponse& response)
{
   std::map<unsigned int, float> feeLevels;
   for (const auto& pair : response.fee_levels()) {
      feeLevels[pair.level()] = pair.fee();
   }
   //TODO
   return true;
}

bool QtQuickAdapter::processWalletsList(const WalletsMessage_WalletsListResponse& response)
{
   std::vector<bs::sync::HDWalletData> wallets;
   for (const auto& wallet : response.wallets()) {
      wallets.push_back(bs::sync::HDWalletData::fromCommonMessage(wallet));
   }
   //TODO
   return true;
}

bool QtQuickAdapter::processUTXOs(const WalletsMessage_UtxoListResponse& response)
{
   std::vector<UTXO> utxos;
   for (const auto& serUtxo : response.utxos()) {
      UTXO utxo;
      utxo.unserialize(BinaryData::fromString(serUtxo));
      utxos.push_back(std::move(utxo));
   }
   //TODO
   return true;
}

bool QtQuickAdapter::processSignTX(const BlockSettle::Common::SignerMessage_SignTxResponse& response)
{
   //TODO
   return true;
}

bool QtQuickAdapter::processZC(const BlockSettle::Common::ArmoryMessage_ZCReceived& zcs)
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
   const auto msgId = pushRequest(user_, userWallets_, msg.SerializeAsString());
   if (!msgId) {
      return false;
   }
   newZCs_.insert(msgId);
   return true;
}

bool QtQuickAdapter::processZCInvalidated(const ArmoryMessage_ZCInvalidated& zcInv)
{
   std::vector<BinaryData> txHashes;
   for (const auto& hashStr : zcInv.tx_hashes()) {
      txHashes.push_back(BinaryData::fromString(hashStr));
   }
   //TODO
   return true;
}

bool QtQuickAdapter::processReservedUTXOs(const WalletsMessage_ReservedUTXOs& response)
{
   std::vector<UTXO> utxos;
   for (const auto& utxoSer : response.utxos()) {
      UTXO utxo;
      utxo.unserialize(BinaryData::fromString(utxoSer));
      utxos.push_back(std::move(utxo));
   }
   //TODO
   return true;
}
