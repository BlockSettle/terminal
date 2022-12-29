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
#include <QClipboard>
#include <QDateTime>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QIcon>
#include <QLockFile>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickWindow>
#include <QPalette>
#include <QStandardPaths>
#include <QThread>
#include <QTimer>
#include <spdlog/spdlog.h>
#include "bip39/bip39.h"
#include "BSMessageBox.h"
#include "BSTerminalSplashScreen.h"
#include "QTXSignRequest.h"
#include "TxOutputsModel.h"
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
   , txTypes_({ tr("All transactions") })
{
   staticLogger = logger;
   addrModel_ = new QmlAddressListModel(logger, this);
   pendingTxModel_ = new TxListModel(logger, this);
   txModel_ = new TxListModel(logger, this);
   expTxByAddrModel_ = new TxListForAddr(logger, this);
   txOutputsModel_ = new TxOutputsModel(logger, this);
   txInputsModel_ = new TxInputsModel(logger, txOutputsModel_, this);
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
   qmlRegisterInterface<QObjectList>("QObjectList");
   qmlRegisterInterface<QTXSignRequest>("QTXSignRequest");
   qmlRegisterInterface<QUTXO>("QUTXO");
   qmlRegisterInterface<QUTXOList>("QUTXOList");
   qmlRegisterInterface<QTxDetails>("QTxDetails");

   QQmlApplicationEngine engine;
   QQuickWindow::setTextRenderType(QQuickWindow::NativeTextRendering);
   rootCtxt_ = engine.rootContext();
   const QFont fixedFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
   rootCtxt_->setContextProperty(QStringLiteral("fixedFont"), fixedFont);
   rootCtxt_->setContextProperty(QLatin1Literal("bsApp"), this);
   rootCtxt_->setContextProperty(QLatin1Literal("addressListModel"), addrModel_);
   rootCtxt_->setContextProperty(QLatin1Literal("pendingTxListModel"), pendingTxModel_);
   rootCtxt_->setContextProperty(QLatin1Literal("txListModel"), txModel_);
   rootCtxt_->setContextProperty(QLatin1Literal("txListByAddrModel"), expTxByAddrModel_);
   rootCtxt_->setContextProperty(QLatin1Literal("txInputsModel"), txInputsModel_);
   rootCtxt_->setContextProperty(QLatin1Literal("txOutputsModel"), txOutputsModel_);

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

   auto comboWalletsList = rootObj_->findChild<QQuickItem*>(QLatin1Literal("walletsComboBox"));
   if (comboWalletsList) {
      QObject::connect((QObject*)comboWalletsList, SIGNAL(activated(int)), this, SLOT(walletSelected(int)));
   }
   comboWalletsList = rootObj_->findChild<QQuickItem*>(QLatin1Literal("receiveWalletsComboBox"));
   if (comboWalletsList) {
      QObject::connect((QObject*)comboWalletsList, SIGNAL(activated(int)), this, SLOT(walletSelected(int)));
   }
   comboWalletsList = rootObj_->findChild<QQuickItem*>(QLatin1Literal("sendWalletsComboBox"));
   if (comboWalletsList) {
      QObject::connect((QObject*)comboWalletsList, SIGNAL(activated(int)), this, SLOT(walletSelected(int)));
   }

   updateStates();

   requestInitialSettings();
   logger_->debug("[QtGuiAdapter::run] initial setup done");
   app.exec();
}

QStringList QtQuickAdapter::txWalletsList() const
{
   QStringList result = { tr("All wallets") };
   result.append(walletsList_);
   return result;
}

ProcessingResult QtQuickAdapter::process(const Envelope &env)
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
   return ProcessingResult::Ignored;
}

bool QtQuickAdapter::processBroadcast(const bs::message::Envelope& env)
{
   if (std::dynamic_pointer_cast<UserTerminal>(env.sender)) {
      switch (env.sender->value<bs::message::TerminalUsers>()) {
      case TerminalUsers::System:
         return (processAdminMessage(env) != ProcessingResult::Ignored);
      case TerminalUsers::Settings:
         return (processSettings(env) != ProcessingResult::Ignored);
      case TerminalUsers::Blockchain:
         return (processBlockchain(env) != ProcessingResult::Ignored);
      case TerminalUsers::Signer:
         return (processSigner(env) != ProcessingResult::Ignored);
      case TerminalUsers::Wallets:
         return (processWallets(env) != ProcessingResult::Ignored);
      default:    break;
      }
   }
   return false;
}

ProcessingResult QtQuickAdapter::processSettings(const Envelope &env)
{
   SettingsMessage msg;
   if (!msg.ParseFromString(env.message)) {
      logger_->error("[{}] failed to parse settings msg #{}", __func__, env.foreignId());
      return ProcessingResult::Error;
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
   return ProcessingResult::Ignored;
}

ProcessingResult QtQuickAdapter::processSettingsGetResponse(const SettingsMessage_SettingsResponse &response)
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
      for (const auto& setting : settings) {
         settingsCache_[static_cast<ApplicationSettings::Setting>(setting.first)] = setting.second;
      }
      emit settingChanged();
   }
   return ProcessingResult::Success;
}

ProcessingResult QtQuickAdapter::processSettingsState(const SettingsMessage_SettingsResponse& response)
{
   ApplicationSettings::State state;
   for (const auto& setting : response.responses()) {
      state[static_cast<ApplicationSettings::Setting>(setting.request().index())] =
         fromResponse(setting);
   }
   //TODO: process setting
   return ProcessingResult::Success;
}

ProcessingResult QtQuickAdapter::processArmoryServers(const SettingsMessage_ArmoryServers& response)
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
   return ProcessingResult::Success;
}

ProcessingResult QtQuickAdapter::processAdminMessage(const Envelope &env)
{
   AdministrativeMessage msg;
   if (!msg.ParseFromString(env.message)) {
      logger_->error("[{}] failed to parse admin msg #{}", __func__, env.foreignId());
      return ProcessingResult::Error;
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
   return ProcessingResult::Success;
}

ProcessingResult QtQuickAdapter::processBlockchain(const Envelope &env)
{
   ArmoryMessage msg;
   if (!msg.ParseFromString(env.message)) {
      logger_->error("[QtGuiAdapter::processBlockchain] failed to parse msg #{}"
         , env.foreignId());
      if (!env.receiver) {
         logger_->debug("[{}] no receiver", __func__);
      }
      return ProcessingResult::Error;
   }
   switch (msg.data_case()) {
   case ArmoryMessage::kLoading:
      loadingComponents_.insert(env.sender->value());
      updateSplashProgress();
      break;
   case ArmoryMessage::kStateChanged:
      armoryState_ = msg.state_changed().state();
      setTopBlock(msg.state_changed().top_block());
      emit armoryStateChanged();
      break;
   case ArmoryMessage::kNewBlock:
      setTopBlock(msg.new_block().top_block());
      break;
   case ArmoryMessage::kWalletRegistered:
      if (msg.wallet_registered().success() && msg.wallet_registered().wallet_id().empty()) {
         walletsReady_ = true;
         WalletsMessage msg;
         msg.set_get_ledger_entries({});
         pushRequest(user_, userWallets_, msg.SerializeAsString());
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
   case ArmoryMessage::kTransactions:
      return processTransactions(env.responseId(), msg.transactions());
   default: return ProcessingResult::Ignored;
   }
   return ProcessingResult::Success;
}

ProcessingResult QtQuickAdapter::processSigner(const Envelope &env)
{
   SignerMessage msg;
   if (!msg.ParseFromString(env.message)) {
      logger_->error("[QtGuiAdapter::processSigner] failed to parse msg #{}"
         , env.foreignId());
      if (!env.receiver) {
         logger_->debug("[{}] no receiver", __func__);
      }
      return ProcessingResult::Error;
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
   case SignerMessage::kCreatedWallet:
      walletsList_.clear();
      logger_->debug("[{}] wallet {} created: {}", __func__    //TODO: show something in the GUI if needed
         , msg.created_wallet().wallet_id(), msg.created_wallet().error_msg());
      break;
   default: return ProcessingResult::Ignored;
   }
   return ProcessingResult::Success;
}

ProcessingResult QtQuickAdapter::processWallets(const Envelope &env)
{
   WalletsMessage msg;
   if (!msg.ParseFromString(env.message)) {
      logger_->error("[{}] failed to parse msg #{}", __func__, env.foreignId());
      return ProcessingResult::Error;
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
      processWalletAddresses(addresses);
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
      return processWalletBalances(env.responseId(), msg.wallet_balances());
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
          WalletsMessage msg;
          msg.set_get_ledger_entries({});
          pushRequest(user_, userWallets_, msg.SerializeAsString());
      }
      break;
   case WalletsMessage::kLedgerEntries:
      return processLedgerEntries(msg.ledger_entries());
   case WalletsMessage::kTxResponse:
      return processTxResponse(env.responseId(), msg.tx_response());
   default: return ProcessingResult::Ignored;
   }
   return ProcessingResult::Success;
}

void QtQuickAdapter::updateStates()
{
   //TODO
}

void QtQuickAdapter::setTopBlock(uint32_t topBlock)
{
   blockNum_ = topBlock;
   pendingTxModel_->setCurrentBlock(blockNum_);
   txModel_->setCurrentBlock(blockNum_);
   expTxByAddrModel_->setCurrentBlock(blockNum_);
   txInputsModel_->setTopBlock(topBlock);
}

//#define DEBUG_LOADING_PROGRESS
void QtQuickAdapter::updateSplashProgress()
{
   std::lock_guard<std::recursive_mutex> lock(mutex_);
   if (createdComponents_.empty()) {
      if (splashScreen_) {
         QMetaObject::invokeMethod(splashScreen_, [this] {
            QTimer::singleShot(100, [this] {
               if (!splashScreen_) {
                  return;
               }
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
         if (!splashScreen_) {
            return;
         }
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
   setReq->set_index(SetIdx_AdvancedTXisDefault);
   setReq->set_type(SettingType_Bool);

   setReq = msgReq->add_requests();
   setReq->set_source(SettingSource_Local);
   setReq->set_index(SetIdx_LogDefault);
   setReq->set_type(SettingType_Strings);

   setReq = msgReq->add_requests();
   setReq->set_source(SettingSource_Local);
   setReq->set_index(SetIdx_LogMessages);
   setReq->set_type(SettingType_Strings);

   setReq = msgReq->add_requests();
   setReq->set_source(SettingSource_Local);
   setReq->set_index(SetIdx_Environment);
   setReq->set_type(SettingType_Int);

   setReq = msgReq->add_requests();
   setReq->set_source(SettingSource_Local);
   setReq->set_index(SetIdx_ArmoryDbIP);
   setReq->set_type(SettingType_String);

   setReq = msgReq->add_requests();
   setReq->set_source(SettingSource_Local);
   setReq->set_index(SetIdx_ArmoryDbPort);
   setReq->set_type(SettingType_String);

   pushRequest(user_, userSettings_, msg.SerializeAsString());
}

void QtQuickAdapter::createWallet(bool primary)
{
   logger_->debug("[{}] primary: {}", __func__, primary);
}

std::string QtQuickAdapter::hdWalletIdByIndex(int index)
{
   if ((index < 0) || (index >= walletsList_.size())) {
      return {};
   }
   const auto& walletName = walletsList_.at(index).toStdString();
   std::string walletId;
   for (const auto& wallet : hdWallets_) {
      if (wallet.second.name == walletName) {
         walletId = wallet.first;
         break;
      }
   }
   return walletId;
}

void QtQuickAdapter::walletSelected(int index)
{
   curWalletIndex_ = index;
   const auto& walletName = walletsList_.at(index).toStdString();
   const auto& walletId = hdWalletIdByIndex(index);
   confWalletBalance_ = unconfWalletBalance_ = totalWalletBalance_ = 0;
   nbUsedWalletAddresses_ = 0;
   WalletsMessage msg;
   msg.set_get_wallet_balances(walletId);
   pushRequest(user_, userWallets_, msg.SerializeAsString(), {}, 10, std::chrono::milliseconds{500});

   addrModel_->clear();
   msg.set_wallet_get(walletId);
   const auto msgId = pushRequest(user_, userWallets_, msg.SerializeAsString());
   walletInfoReq_[msgId] = walletName;
   walletNames_[walletId] = walletName;
}

void QtQuickAdapter::processWalletLoaded(const bs::sync::WalletInfo &wi)
{
   const bool isInitialLoad = hdWallets_.empty();
   hdWallets_[*wi.ids.cbegin()] = wi;
   logger_->debug("[QtQuickAdapter::processWalletLoaded] {}", wi.name);
   QMetaObject::invokeMethod(this, [this, isInitialLoad, walletName = QString::fromStdString(wi.name)] {
      walletsList_.push_back(walletName);
      if (isInitialLoad) {
         walletSelected(0);
      }
      emit walletsListChanged();
   });
}

ProcessingResult QtQuickAdapter::processWalletData(bs::message::SeqId msgId
   , const WalletsMessage_WalletData& response)
{
   const auto& itReq = walletInfoReq_.find(msgId);
   if (itReq != walletInfoReq_.end()) {
      std::unordered_set<std::string> walletIds;
      for (const auto& addr : response.used_addresses()) {
         walletIds.insert(addr.wallet_id());
         if (!addr.comment().empty()) {
            try {
               const auto& address = bs::Address::fromAddressString(addr.address());
               addrComments_[address] = addr.comment();
            }
            catch (const std::exception&) {}
         }
      }
      for (const auto& walletId : walletIds) {
         walletNames_[walletId] = itReq->second;
      }
      walletInfoReq_.erase(itReq);
   }
   for (const auto& addr : response.used_addresses()) {
      addrModel_->addRow({ QString::fromStdString(addr.address()), {}, {}
         , QString::fromStdString(addr.comment().empty() ? addr.index() : addr.comment()) });
   }
   return ProcessingResult::Success;
}

ProcessingResult QtQuickAdapter::processWalletBalances(bs::message::SeqId responseId
   , const WalletsMessage_WalletBalances &response)
{
   //logger_->debug("[{}] {}", __func__, response.DebugString());
   if (response.wallet_id() != hdWalletIdByIndex(curWalletIndex_)) {
      return ProcessingResult::Ignored;
   }
   totalWalletBalance_ = response.total_balance();
   confWalletBalance_ = response.spendable_balance();
   unconfWalletBalance_ = response.unconfirmed_balance();
   nbUsedWalletAddresses_ = response.nb_addresses();

   for (const auto& addrBal : response.address_balances()) {
      addrModel_->updateRow(BinaryData::fromString(addrBal.address()), addrBal.total_balance(), addrBal.tx_count());
   }
   emit walletBalanceChanged();
   return ProcessingResult::Success;
}

ProcessingResult QtQuickAdapter::processTXDetails(bs::message::SeqId msgId
   , const WalletsMessage_TXDetailsResponse &response)
{
   //logger_->debug("[{}] {}", __func__, response.DebugString());
   for (const auto &resp : response.responses()) {
      bs::sync::TXWalletDetails txDet{ BinaryData::fromString(resp.tx_hash()), resp.hd_wallet_id()
         , resp.wallet_name(), static_cast<bs::core::wallet::Type>(resp.wallet_type())
         , resp.wallet_symbol(), static_cast<bs::sync::Transaction::Direction>(resp.direction())
         , resp.comment(), resp.valid(), resp.amount() };
      if (!response.error_msg().empty()) {
         txDet.comment = response.error_msg();
      }
      if (!resp.comment().empty()) {
         pendingTxModel_->setTxComment(resp.tx_hash(), txDet.comment);
         txModel_->setTxComment(resp.tx_hash(), txDet.comment);
      }

      const auto &ownTxHash = BinaryData::fromString(resp.tx_hash());
      try {
         if (!resp.tx().empty()) {
            Tx tx(BinaryData::fromString(resp.tx()));
            if (tx.isInitialized()) {
               txDet.tx = std::move(tx);
               txDet.tx.setTxHeight(resp.tx_height());
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
      for (const auto& addr : resp.own_addresses()) {
         try {
            txDet.ownAddresses.push_back(bs::Address::fromAddressString(addr));
         }
         catch (const std::exception&) {}
      }
      for (const auto& walletId : resp.wallet_ids()) {
         txDet.walletIds.insert(walletId);
      }

      const auto& itTxDet = txDetailReqs_.find(msgId);
      if (itTxDet == txDetailReqs_.end()) {
         pendingTxModel_->setDetails(txDet);
         txModel_->setDetails(txDet);
      }
      else {
         itTxDet->second->setDetails(txDet); // shouldn't be more than one entry
         itTxDet->second->setCurBlock(blockNum_);
         txDetailReqs_.erase(itTxDet);
      }
   }
   return ProcessingResult::Success;
}

ProcessingResult QtQuickAdapter::processLedgerEntries(const LedgerEntries &response)
{
   //logger_->debug("[{}] {}", __func__, response.DebugString());
   WalletsMessage msg;
   auto msgReq = msg.mutable_tx_details_request();
   std::vector<bs::TXEntry> entries;
   for (const auto &entry : response.entries()) {
      auto txReq = msgReq->add_requests();
      txReq->set_tx_hash(entry.tx_hash());
      txReq->set_value(entry.value());

      bs::TXEntry txEntry;
      txEntry.txHash = BinaryData::fromString(entry.tx_hash());
      txEntry.value = entry.value();
      txEntry.blockNum = entry.block_num();
      txEntry.txTime = entry.tx_time();
      txEntry.isRBF = entry.rbf();
      txEntry.isChainedZC = entry.chained_zc();
      txEntry.nbConf = entry.nb_conf();
      for (const auto &walletId : entry.wallet_ids()) {
         txReq->add_wallet_ids(walletId);
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
   for (const auto& entry : entries) {
      if (entry.nbConf <= 6) {
         pendingTxModel_->addRow(entry);
      }
   }
   txModel_->addRows(entries);
   pushRequest(user_, userWallets_, msg.SerializeAsString());
   return ProcessingResult::Success;
}

QStringList QtQuickAdapter::settingEnvironments() const
{
   return { tr("Main"), tr("Test") };
}

QStringList QtQuickAdapter::newSeedPhrase()
{
   auto seed = CryptoPRNG::generateRandom(16);
   std::vector<uint8_t> seedData;
   for (int i = 0; i < (int)seed.getSize(); ++i) {
      seedData.push_back(seed.getPtr()[i]);
   }
   const auto& words = BIP39::create_mnemonic(seedData);
   QStringList result;
   for (const auto& word : words) {
      result.append(QString::fromStdString(word));
   }
   return result;
}

void QtQuickAdapter::copySeedToClipboard(const QStringList& seed)
{
   const auto& str = seed.join(QLatin1Char(' '));
   QGuiApplication::clipboard()->setText(str);
}

void QtQuickAdapter::createWallet(const QString& name, const QStringList& seed
   , const QString& password)
{
   logger_->debug("[{}] {}", __func__, name.toStdString());
   BIP39::word_list words;
   for (const auto& w : seed) {
      words.add(w.toStdString());
   }
   SignerMessage msg;
   auto msgReq = msg.mutable_create_wallet();
   msgReq->set_name(name.toStdString());
   //msgReq->set_xpriv_key(seedData.xpriv);
   const auto binSeed = BIP39::seed_from_mnemonic(words);
   msgReq->set_seed(binSeed.toBinStr());
   msgReq->set_password(password.toStdString());
   pushRequest(user_, userSigner_, msg.SerializeAsString());
}

void QtQuickAdapter::importWallet(const QString& name, const QStringList& seed
   , const QString& password)
{
   logger_->debug("[{}] {}", __func__, name.toStdString());
   BIP39::word_list words;
   for (const auto& w : seed) {
      words.add(w.toStdString());
   }
   SignerMessage msg;
   auto msgReq = msg.mutable_import_wallet();
   msgReq->set_name(name.toStdString());
   //msgReq->set_description(seedData.description);
   //msgReq->set_xpriv_key(seedData.xpriv);
   const auto binSeed = BIP39::seed_from_mnemonic(words);
   msgReq->set_seed(binSeed.toBinStr());
   msgReq->set_password(password.toStdString());
   pushRequest(user_, userSigner_, msg.SerializeAsString());
}

void QtQuickAdapter::generateNewAddress(int walletIndex, bool isNative)
{
   const auto& hdWalletId = hdWalletIdByIndex(walletIndex);
   logger_->debug("[{}] #{}: {}", __func__, walletIndex, hdWalletId);
   //TODO: find leaf walletId depending on isNative
   WalletsMessage msg;
   msg.set_create_ext_address(hdWalletId);
   pushRequest(user_, userWallets_, msg.SerializeAsString());
}

void QtQuickAdapter::copyAddressToClipboard(const QString& addr)
{
   QGuiApplication::clipboard()->setText(addr);
   if (!generatedAddress_.empty()) {
      generatedAddress_.clear();
      emit addressGenerated();
   }
}

QTXSignRequest* QtQuickAdapter::createTXSignRequest(int walletIndex, const QString& recvAddr
   , double amount, double fee, const QString& comment, QUTXOList* utxos)
{
   WalletsMessage msg;
   auto msgReq = msg.mutable_tx_request();
   msgReq->set_hd_wallet_id(hdWalletIdByIndex(walletIndex));
   if (!recvAddr.isEmpty() && (amount > 0)) {
      auto msgOut = msgReq->add_outputs();
      msgOut->set_address(recvAddr.toStdString());
      msgOut->set_amount(amount);
   }
   else {
      const auto& recipients = txOutputsModel_->recipients();
      for (const auto& recip : recipients) {
         try {
            const auto& addr = bs::Address::fromRecipient(recip);
            auto msgOut = msgReq->add_outputs();
            msgOut->set_address(addr.display());
            msgOut->set_amount(recip->getValue() / BTCNumericTypes::BalanceDivider);
         }
         catch (const std::exception& e) {
            logger_->error("[{}] {}", __func__, e.what());
         }
      }
   }
   msgReq->set_fee_per_byte(fee);
   if (!comment.isEmpty()) {
      msgReq->set_comment(comment.toStdString());
   }
   if (utxos) {
      for (const auto& qUtxo : utxos->data()) {
         msgReq->add_utxos(qUtxo->utxo().serialize().toBinStr());
      }
   }
   const auto msgId = pushRequest(user_, userWallets_, msg.SerializeAsString());
   const auto txReq = new QTXSignRequest(this);
   txReqs_[msgId] = txReq;
   return txReq;
}

void QtQuickAdapter::getUTXOsForWallet(int walletIndex)
{
   logger_->debug("[{}] #{}", __func__, walletIndex);
   txInputsModel_->clear();
   WalletsMessage msg;
   auto msgReq = msg.mutable_get_utxos();
   msgReq->set_wallet_id(hdWalletIdByIndex(walletIndex));
   pushRequest(user_, userWallets_, msg.SerializeAsString());
}

QTxDetails* QtQuickAdapter::getTXDetails(const QString& txHash)
{
   auto txBinHash = BinaryData::CreateFromHex(txHash.trimmed().toStdString());
   txBinHash.swapEndian();
   if (txBinHash.getSize() != 32) {
      return new QTxDetails({}, this);
   }
   WalletsMessage msg;
   auto msgReq = msg.mutable_tx_details_request();
   auto txReq = msgReq->add_requests();
   txReq->set_tx_hash(txBinHash.toBinStr());
   const auto msgId = pushRequest(user_, userWallets_, msg.SerializeAsString());
   const auto txDet = new QTxDetails(txBinHash, this);
   txDetailReqs_[msgId] = txDet;
   return txDet;
}

ProcessingResult QtQuickAdapter::processAddressHist(const ArmoryMessage_AddressHistory& response)
{
   bs::Address addr;
   try {
      addr = std::move(bs::Address::fromAddressString(response.address()));
   }
   catch (const std::exception& e) {
      logger_->error("[{}] invalid address: {}", __func__, e.what());
      return ProcessingResult::Error;
   }
   ArmoryMessage msg;
   auto msgReq = msg.mutable_get_txs_by_hash();
   std::vector<bs::TXEntry> entries;
   for (const auto& entry : response.entries()) {
      msgReq->add_tx_hashes(entry.tx_hash());
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
   expTxByAddrModel_->addRows(entries);
   const auto msgId = pushRequest(user_, userBlockchain_, msg.SerializeAsString());
   expTxAddrReqs_.insert(msgId);
   return ProcessingResult::Success;
}

ProcessingResult QtQuickAdapter::processFeeLevels(const ArmoryMessage_FeeLevelsResponse& response)
{
   std::map<unsigned int, float> feeLevels;
   for (const auto& pair : response.fee_levels()) {
      feeLevels[pair.level()] = pair.fee();
   }
   //TODO
   return ProcessingResult::Success;
}

ProcessingResult QtQuickAdapter::processWalletsList(const WalletsMessage_WalletsListResponse& response)
{
   logger_->debug("[QtQuickAdapter::processWalletsList] {}", response.DebugString());
   walletsList_.clear();
   for (const auto& wallet : response.wallets()) {
      const auto& hdWallet = bs::sync::HDWalletData::fromCommonMessage(wallet);
      walletsList_.push_back(QString::fromStdString(hdWallet.name));
   }
   emit walletsListChanged();
   return ProcessingResult::Success;
}

ProcessingResult QtQuickAdapter::processUTXOs(const WalletsMessage_UtxoListResponse& response)
{
   std::vector<UTXO> utxos;
   for (const auto& serUtxo : response.utxos()) {
      UTXO utxo;
      utxo.unserialize(BinaryData::fromString(serUtxo));
      utxos.push_back(std::move(utxo));
   }
   txInputsModel_->addUTXOs(utxos);
   return ProcessingResult::Success;
}

ProcessingResult QtQuickAdapter::processSignTX(const BlockSettle::Common::SignerMessage_SignTxResponse& response)
{
   const auto& signedTX = BinaryData::fromString(response.signed_tx());
   logger_->debug("[{}] signed TX size: {}", __func__, signedTX.getSize());

   ArmoryMessage msg;
   auto msgReq = msg.mutable_tx_push();
   //msgReq->set_push_id(id);
   auto msgTx = msgReq->add_txs_to_push();
   msgTx->set_tx(response.signed_tx());
   //not adding TX hashes atm
   pushRequest(user_, userBlockchain_, msg.SerializeAsString());
   return ProcessingResult::Success;
}

ProcessingResult QtQuickAdapter::processZC(const BlockSettle::Common::ArmoryMessage_ZCReceived& zcs)
{
   logger_->debug("[{}] {}", __func__, zcs.DebugString());
   WalletsMessage msg;
   auto msgReq = msg.mutable_tx_details_request();
   for (const auto& zcEntry : zcs.tx_entries()) {
      auto txReq = msgReq->add_requests();
      txReq->set_tx_hash(zcEntry.tx_hash());
      for (const auto& walletId : zcEntry.wallet_ids()) {
         txReq->add_wallet_ids(walletId);
      }
      txReq->set_value(zcEntry.value());

      bs::TXEntry txEntry;
      txEntry.txHash = BinaryData::fromString(zcEntry.tx_hash());
      txEntry.value = zcEntry.value();
      txEntry.blockNum = blockNum_;
      txEntry.txTime = zcEntry.tx_time();
      txEntry.isRBF = zcEntry.rbf();
      txEntry.isChainedZC = zcEntry.chained_zc();
      txEntry.nbConf = zcEntry.nb_conf();
      for (const auto& walletId : zcEntry.wallet_ids()) {
         txEntry.walletIds.insert(walletId);
      }
      for (const auto& addrStr : zcEntry.addresses()) {
         try {
            const auto& addr = bs::Address::fromAddressString(addrStr);
            txEntry.addresses.push_back(addr);
         }
         catch (const std::exception&) {}
      }
      pendingTxModel_->prependRow(txEntry);
      txModel_->prependRow(txEntry);
   }
   pushRequest(user_, userWallets_, msg.SerializeAsString());
   return ProcessingResult::Success;
}

ProcessingResult QtQuickAdapter::processZCInvalidated(const ArmoryMessage_ZCInvalidated& zcInv)
{
   std::vector<BinaryData> txHashes;
   for (const auto& hashStr : zcInv.tx_hashes()) {
      txHashes.push_back(BinaryData::fromString(hashStr));
   }
   //TODO
   return ProcessingResult::Success;
}

bs::message::ProcessingResult QtQuickAdapter::processTransactions(bs::message::SeqId msgId
   , const ArmoryMessage_Transactions& response)
{
   std::vector<Tx> result;
   std::set<BinaryData> inHashes;
   for (const auto& txData : response.transactions()) {
      Tx tx(BinaryData::fromString(txData.tx()));
      tx.setTxHeight(txData.height());
      for (int i = 0; i < tx.getNumTxIn(); ++i) {
         const auto& in = tx.getTxInCopy(i);
         const OutPoint op = in.getOutPoint();
         inHashes.insert(op.getTxHash());
      }
      result.emplace_back(std::move(tx));
   }
   const auto& itExpTxAddr = expTxAddrReqs_.find(msgId);
   if (itExpTxAddr != expTxAddrReqs_.end()) {
      ArmoryMessage msg;
      auto msgReq = msg.mutable_get_txs_by_hash();
      for (const auto& inHash : inHashes) {
         msgReq->add_tx_hashes(inHash.toBinStr());
      }
      expTxAddrReqs_.erase(itExpTxAddr);
      expTxByAddrModel_->setDetails(result);
      const auto msgIdReq = pushRequest(user_, userBlockchain_, msg.SerializeAsString());
      expTxAddrInReqs_.insert(msgIdReq);
   }
   const auto& itExpTxAddrIn = expTxAddrInReqs_.find(msgId);
   if (itExpTxAddrIn != expTxAddrInReqs_.end()) {
      expTxAddrInReqs_.erase(itExpTxAddrIn);
      expTxByAddrModel_->setInputs(result);
   }
   return bs::message::ProcessingResult::Success;
}

ProcessingResult QtQuickAdapter::processReservedUTXOs(const WalletsMessage_ReservedUTXOs& response)
{
   std::vector<UTXO> utxos;
   for (const auto& utxoSer : response.utxos()) {
      UTXO utxo;
      utxo.unserialize(BinaryData::fromString(utxoSer));
      utxos.push_back(std::move(utxo));
   }
   //TODO
   return ProcessingResult::Success;
}

void QtQuickAdapter::processWalletAddresses(const std::vector<bs::sync::Address>& addresses)
{
   if (addresses.empty()) {
      return;
   }
   const auto lastAddr = addresses.at(addresses.size() - 1);
   logger_->debug("[{}] last address: {}", __func__, lastAddr.address.display());
   addrModel_->addRow({ QString::fromStdString(lastAddr.address.display()), {}, {}
      , QString::fromStdString(lastAddr.index) });
   generatedAddress_ = lastAddr.address;
   emit addressGenerated();
}

bs::message::ProcessingResult QtQuickAdapter::processTxResponse(bs::message::SeqId msgId
   , const WalletsMessage_TxResponse& response)
{
   const auto& itReq = txReqs_.find(msgId);
   if (itReq == txReqs_.end()) {
      logger_->error("[{}] unknown request #{}", __func__, msgId);
      return bs::message::ProcessingResult::Error;
   }
   auto qReq = itReq->second;
   txReqs_.erase(itReq);
   if (!response.error_text().empty()) {
      logger_->error("[{}] {}", __func__, response.error_text());
      qReq->setError(QString::fromStdString(response.error_text()));
      return bs::message::ProcessingResult::Success;
   }
   const auto txReq = bs::signer::pbTxRequestToCore(response.tx_sign_request(), logger_);
   qReq->setTxSignReq(txReq);
   return bs::message::ProcessingResult::Success;
}

QVariant QtQuickAdapter::getSetting(ApplicationSettings::Setting s) const
{
   try {
      return settingsCache_.at(s);
   }
   catch (const std::exception&) {}
   return {};
}

QString QtQuickAdapter::getSettingStringAt(ApplicationSettings::Setting s, int idx)
{
   const auto& list = getSetting(s).toStringList();
   if ((idx >= 0) && (idx < list.size())) {
      return list.at(idx);
   }
   return {};
}

void QtQuickAdapter::setSetting(ApplicationSettings::Setting s, const QVariant& val)
{
   if (settingsCache_.empty()) {
      return;
   }
   try {
      if (settingsCache_.at(s) == val) {
         return;
      }
   }
   catch (const std::exception&) {}
   logger_->debug("[{}] {} = {}", __func__, (int)s, val.toString().toStdString());
   settingsCache_[s] = val;
   SettingsMessage msg;
   auto msgReq = msg.mutable_put_request();
   auto setResp = msgReq->add_responses();
   auto setReq = setResp->mutable_request();
   setReq->set_source(SettingSource_Local);
   setReq->set_index(static_cast<SettingIndex>(s));
   setFromQVariant(val, setReq, setResp);

   pushRequest(user_, userSettings_, msg.SerializeAsString());
}

void QtQuickAdapter::resetArmoryConnection()
{
}

void QtQuickAdapter::signAndBroadcast(QTXSignRequest* txReq, const QString& password)
{
   if (!txReq) {
      logger_->error("[{}] no TX request passed", __func__);
      return;
   }
   const auto& txSignReq = txReq->txReq();
   SignerMessage msg;
   auto msgReq = msg.mutable_sign_tx_request();
   //msgReq->set_id(id);
   *msgReq->mutable_tx_request() = bs::signer::coreTxRequestToPb(txSignReq);
   msgReq->set_sign_mode((int)SignContainer::TXSignMode::Full);
   //msgReq->set_keep_dup_recips(keepDupRecips);
   msgReq->set_passphrase(password.toStdString());
   pushRequest(user_, userSigner_, msg.SerializeAsString());
}

int QtQuickAdapter::startSearch(const QString& s)
{
   const auto& trimmed = s.trimmed().toStdString();
   bs::Address address;
   try {
      address = bs::Address::fromAddressString(trimmed);
      if (address.isValid()) {
         expTxByAddrModel_->clear();
         ArmoryMessage msg;
         msg.set_get_address_history(address.display());
         pushRequest(user_, userBlockchain_, msg.SerializeAsString());
         return 1;
      }
   }
   catch (const std::exception&) {}

   if (trimmed.length() == 64) { // potential TX hash in hex
      const auto& txId = BinaryData::CreateFromHex(trimmed);
      if (txId.getSize() == 32) {   // valid TXid
         return 2;
      }
   }
   return 0;
}