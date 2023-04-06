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
#include <QQmlContext>
#include <QQuickImageProvider>
#include <QQuickItem>
#include <QQuickWindow>
#include <QPalette>
#include <QStandardPaths>
#include <QThread>
#include <QTimer>
#include <spdlog/spdlog.h>
#include "ArmoryServersModel.h"
#include "bip39/bip39.h"
#include "BSMessageBox.h"
#include "BSTerminalSplashScreen.h"
#include "FeeSuggModel.h"
#include "hwdevicemanager.h"
#include "QTXSignRequest.h"
#include "TxOutputsModel.h"
#include "TxInputsSelectedModel.h"
#include "SettingsAdapter.h"
#include "SystemFileUtils.h"
#include "Wallets/ProtobufHeadlessUtils.h"
#include "WalletBalancesModel.h"
#include "TransactionFilterModel.h"
#include "TransactionForAddressFilterModel.h"
#include "viewmodels/WalletPropertiesVM.h"
#include "PendingTransactionFilterModel.h"
#include "Utils.h"
#include "AddressFilterModel.h"
#include "viewmodels/plugins/PluginsListModel.h"
#include "LeverexPlugin.h"
#include "SideshiftPlugin.h"
#include "SideswapPlugin.h"

#include "hardware_wallet.pb.h"
#include "terminal.pb.h"

using namespace BlockSettle;
using namespace BlockSettle::Common;
using namespace BlockSettle::Terminal;
using namespace bs::message;


namespace {
   const int kMinPasswordLength = 6;

   std::shared_ptr<spdlog::logger> staticLogger;

   static inline QString encTypeToString(bs::wallet::EncryptionType enc)
   {
      switch (enc) {
         case bs::wallet::EncryptionType::Unencrypted :
            return QObject::tr("Unencrypted");

         case bs::wallet::EncryptionType::Password :
            return QObject::tr("Password");

         case bs::wallet::EncryptionType::Auth :
            return QObject::tr("Auth eID");

         case bs::wallet::EncryptionType::Hardware :
            return QObject::tr("Hardware");
      };
      return QObject::tr("Unknown");
   }
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

class QRImageProvider : public QQuickImageProvider
{
public:
   QRImageProvider() : QQuickImageProvider(QQuickImageProvider::Pixmap)
   {}

   QPixmap requestPixmap(const QString& id, QSize* size, const QSize& requestedSize) override
   {
      const int sz = std::max(requestedSize.width(), requestedSize.height());
      if (size) {
         *size = QSize(sz, sz);
      }
      return UiUtils::getQRCode(id, sz);
   }
};


QtQuickAdapter::QtQuickAdapter(const std::shared_ptr<spdlog::logger> &logger)
   : QObject(nullptr), logger_(logger)
   , userSettings_(std::make_shared<UserTerminal>(TerminalUsers::Settings))
   , userWallets_(std::make_shared<UserTerminal>(TerminalUsers::Wallets))
   , userBlockchain_(std::make_shared<UserTerminal>(TerminalUsers::Blockchain))
   , userSigner_(std::make_shared<UserTerminal>(TerminalUsers::Signer))
   , userHWW_(bs::message::UserTerminal::create(bs::message::TerminalUsers::HWWallets))
   , txTypes_({ tr("All transactions"), tr("Received"), tr("Sent"), tr("Internal") })
   , settingsController_(std::make_shared<SettingsController>())
   , addressFilterModel_(std::make_unique<AddressFilterModel>(settingsController_))
   , transactionFilterModel_(std::make_unique<TransactionFilterModel>(settingsController_))
   , pluginsListModel_(std::make_unique<PluginsListModel>())
{
   staticLogger = logger;
   addrModel_ = new QmlAddressListModel(logger, this);
   txModel_ = new TxListModel(logger, this);
   expTxByAddrModel_ = new TxListForAddr(logger, this);
   txOutputsModel_ = new TxOutputsModel(logger, this);
   txInputsModel_ = new TxInputsModel(logger, txOutputsModel_, this);
   txInputsSelectedModel_ = new TxInputsSelectedModel(this);
   txInputsSelectedModel_->setSourceModel(txInputsModel_);
   hwDeviceModel_ = new HwDeviceModel(logger, this);
   walletBalances_ = new WalletBalancesModel(logger, this);
   feeSuggModel_ = new FeeSuggestionModel(logger, this);
   walletPropertiesModel_ = std::make_unique<qtquick_gui::WalletPropertiesVM>(logger);

   addressFilterModel_->setSourceModel(addrModel_);
   transactionFilterModel_->setSourceModel(txModel_);

   connect(settingsController_.get(), &SettingsController::changed, this, [this](ApplicationSettings::Setting key)
   {
      setSetting(key, settingsController_->getParam(key));
   });


   connect(settingsController_.get(), &SettingsController::reset, this, [this]()
   {
      if (settingsController_->hasParam(ApplicationSettings::Setting::SelectedWallet)) {
         emit requestWalletSelection(settingsController_->getParam(ApplicationSettings::Setting::SelectedWallet).toInt());
      }
   });

   connect(walletBalances_, &WalletBalancesModel::walletSelected, this, [this](int index)
   {
        walletSelected(index);
   });
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
   QApplication::setWindowIcon(QIcon(QStringLiteral(":/images/terminal.ico")));

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
   qmlRegisterInterface<ArmoryServersModel>("ArmoryServersModel");
   qmlRegisterUncreatableMetaObject(WalletBalance::staticMetaObject, "wallet.balance"
      , 1, 0, "WalletBalance", tr("Error: only enums"));
   qmlRegisterType<TransactionForAddressFilterModel>("terminal.models", 1, 0, "TransactionForAddressFilterModel");
   qmlRegisterType<PendingTransactionFilterModel>("terminal.models", 1, 0, "PendingTransactionFilterModel");
   qmlRegisterUncreatableMetaObject(qtquick_gui::WalletPropertiesVM::staticMetaObject, "terminal.models"
      , 1, 0, "WalletPropertiesVM", tr("Error: only enums"));
//   qmlRegisterType<qtquick_gui::WalletPropertiesVM>("terminal.models", 1, 0, "WalletPropertiesVM");
   qmlRegisterUncreatableMetaObject(Transactions::staticMetaObject, "terminal.models" 
      , 1, 0, "Transactions", tr("Error: only enums"));
   qmlRegisterUncreatableMetaObject(TxListModel::staticMetaObject, "terminal.models"
      , 1, 0, "TxListModel", tr("Error: only enums"));
   qmlRegisterUncreatableMetaObject(QmlAddressListModel::staticMetaObject, "terminal.models"
      , 1, 0, "QmlAddressListModel", tr("Error: only enums"));
   qmlRegisterUncreatableMetaObject(ArmoryServersModel::staticMetaObject, "terminal.models"
      , 1, 0, "ArmoryServersModel", tr("Error: only enums"));

   //need to read files in qml
   qputenv("QML_XHR_ALLOW_FILE_READ", QByteArray("1"));

   QQmlApplicationEngine engine;
   QQuickWindow::setTextRenderType(QQuickWindow::NativeTextRendering);
   rootCtxt_ = engine.rootContext();
   const QFont fixedFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
   rootCtxt_->setContextProperty(QStringLiteral("fixedFont"), fixedFont);
   rootCtxt_->setContextProperty(QLatin1Literal("bsApp"), this);
   rootCtxt_->setContextProperty(QLatin1Literal("addressListModel"), addrModel_);
   rootCtxt_->setContextProperty(QLatin1Literal("txListModel"), txModel_);
   rootCtxt_->setContextProperty(QLatin1Literal("txListByAddrModel"), expTxByAddrModel_);
   rootCtxt_->setContextProperty(QLatin1Literal("txInputsModel"), txInputsModel_);
   rootCtxt_->setContextProperty(QLatin1Literal("txInputsSelectedModel"), txInputsSelectedModel_);
   rootCtxt_->setContextProperty(QLatin1Literal("txOutputsModel"), txOutputsModel_);
   rootCtxt_->setContextProperty(QLatin1Literal("hwDeviceModel"), hwDeviceModel_);
   rootCtxt_->setContextProperty(QLatin1Literal("walletBalances"), walletBalances_);
   rootCtxt_->setContextProperty(QLatin1Literal("feeSuggestions"), feeSuggModel_);
   rootCtxt_->setContextProperty(QLatin1Literal("addressFilterModel"), addressFilterModel_.get());
   rootCtxt_->setContextProperty(QLatin1Literal("transactionFilterModel"), transactionFilterModel_.get());
   rootCtxt_->setContextProperty(QLatin1Literal("pluginsListModel"), pluginsListModel_.get());
   engine.addImageProvider(QLatin1Literal("QR"), new QRImageProvider);

   connect(&engine, &QQmlApplicationEngine::objectCreated,
           [this]() {
      if (nWalletsLoaded_ >= 0) {
         emit walletsLoaded(nWalletsLoaded_);
      }
   });

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
   loadPlugins(engine);

   logger_->debug("[QtGuiAdapter::run] initial setup done");
   app.exec();
}

QStringList QtQuickAdapter::txWalletsList() const
{
   QStringList result = { tr("All wallets") };
   result.append(walletBalances_->wallets());
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
      case TerminalUsers::HWWallets:
         return processHWW(env);
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
      case TerminalUsers::HWWallets:
         return (processHWW(env) != ProcessingResult::Ignored);
      default:    break;
      }
   }
   return false;
}

void QtQuickAdapter::onArmoryServerSelected(int index)
{
   if (armoryServerIndex_ == index) {
      return;
   }
   armoryServerIndex_ = index;
   auto model = qobject_cast<ArmoryServersModel*>(sender());
   if (!model) {
      logger_->error("[{}] invalid sender", __func__);
      return;
   }

   armoryState_ = 0;
   emit armoryStateChanged();

   const auto newNetType = model->data(index).netType;
   if (netType_ != newNetType) {
      if (txModel_) {
         txModel_->clear();
      }
      netType_ = newNetType;
      emit networkTypeChanged();
   }
   
   logger_->debug("[{}] #{}", __func__, index);
   SettingsMessage msg;
   msg.set_set_armory_server(index);
   pushRequest(user_, userSettings_, msg.SerializeAsString());
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
      return processArmoryServers(env.responseId(), msg.armory_servers());
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
         [[fallthrough]];

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
   if (createdWalletId_.empty()) {
      QMetaObject::invokeMethod(this, [this] { settingsController_->resetCache(settingsCache_); });
   }
   else {
       createdWalletId_.clear();
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

ProcessingResult QtQuickAdapter::processArmoryServers(bs::message::SeqId msgId
   , const SettingsMessage_ArmoryServers& response)
{
   armoryServerIndex_ = response.idx_current();
   logger_->debug("[{}] current={}, connected={}", __func__, response.idx_current()
      , response.idx_connected());
   const auto& itReq = armoryServersReq_.find(msgId);
   if (itReq == armoryServersReq_.end()) {
      logger_->warn("[{}] unknown request #{}", __func__, msgId);
      return ProcessingResult::Error;
   }
   std::vector<ArmoryServer> servers;
   for (const auto& server : response.servers()) {
      servers.push_back({ QString::fromStdString(server.server_name())
         , static_cast<NetworkType>(server.network_type())
         , QString::fromStdString(server.server_address())
         , std::stoi(server.server_port()), QString::fromStdString(server.server_key())
         , SecureBinaryData::fromString(server.password())
         , server.run_locally(), server.one_way_auth() });
   }
   itReq->second->setData(response.idx_current(), response.idx_connected(), servers);
   armoryServersReq_.erase(itReq);
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
      netType_ = static_cast<NetworkType>(msg.state_changed().net_type());
      setTopBlock(msg.state_changed().top_block());
      emit armoryStateChanged();
      break;
   case ArmoryMessage::kNewBlock:
      setTopBlock(msg.new_block().top_block());
      break;
   case ArmoryMessage::kWalletRegistered:
      if (msg.wallet_registered().success() && msg.wallet_registered().wallet_id().empty()) {
         logger_->debug("wallets ready");
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
   case ArmoryMessage::kTxPushResult:
      if (msg.tx_push_result().result() != ArmoryMessage::PushTxSuccess) {
         emit showError(tr("TX broadcast failed: %1")
            .arg(QString::fromStdString(msg.tx_push_result().error_message())));
      }
      break;
   case ArmoryMessage::kUtxos:
      return processUTXOs(env.responseId(), msg.utxos());
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
      return processWalletDeleted(msg.wallet_deleted());
   case SignerMessage::kCreatedWallet:
      createdWalletId_ = msg.created_wallet().wallet_id();
      walletBalances_->clear();
      addrModel_->reset(createdWalletId_);
      walletBalances_->setCreatedWalletId(createdWalletId_);
      emit showSuccess(tr("Your wallet has successfully been created"));
      break;
   case SignerMessage::kWalletPassChanged:
      if (!msg.wallet_pass_changed()) {
         emit showFail(tr("Incorrect password"), tr("The password you entered is incorrect"));
      }
      else {
         emit successChangePassword();
      }
      break;
   case SignerMessage::kExportWoWalletResponse:
      if (msg.export_wo_wallet_response().empty()) {
         emit showError(tr("WO wallet export failed\nsee log for details"));
      }
      else {
         emit successExport(QString::fromStdString(msg.export_wo_wallet_response()));
      }
      break;
   case SignerMessage::kWalletSeed:
      return processWalletSeed(msg.wallet_seed());
   case SignerMessage::kWalletsReset:
      hdWallets_.clear();
      if (addrModel_) {
         addrModel_->reset({});
      }
      if (walletBalances_) {
         walletBalances_->clear();
      }
      walletsReady_ = false;
      break;
   case SignerMessage::kWalletsListUpdated:
   default: return ProcessingResult::Ignored;
   }
   return ProcessingResult::Success;
}

#include "StringUtils.h"
ProcessingResult QtQuickAdapter::processWallets(const Envelope &env)
{
   WalletsMessage msg;
   if (!msg.ParseFromString(env.message)) {
      logger_->error("[{}] failed to parse msg #{} (response#: {}, to {})\n{}", __func__
         , env.foreignId(), env.responseId(), (env.receiver ? env.receiver->name() : "<null>")
         , bs::toHex(env.message));
      return ProcessingResult::Error;
   }
   switch (msg.data_case()) {
   case WalletsMessage::kLoading:
      loadingComponents_.insert(env.sender->value());
      updateSplashProgress();
      break;

   case WalletsMessage::kReady:
      nWalletsLoaded_ = msg.ready();
      requestPostLoadingSettings();
      emit walletsLoaded(msg.ready());
      logger_->debug("[{}] loaded {} wallet[s]", __func__, msg.ready());
      {
         if (createdWalletId_.empty()) {
            if (settingsController_->hasParam(ApplicationSettings::Setting::SelectedWallet)) {
               const int lastIdx = settingsController_->getParam(ApplicationSettings::Setting::SelectedWallet).toInt();
               if ((lastIdx >= 0) && (lastIdx < nWalletsLoaded_)) {
                  emit requestWalletSelection(lastIdx);
               }
               else if (nWalletsLoaded_ > 0) {
                  emit requestWalletSelection(0);
               }
            }
         }
      }
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
      if (!msg.wallet_deleted().id_size()) {
         showError(tr("Wallet deletion failed"));
         break;
      }
      //const auto& wi = bs::sync::WalletInfo::fromCommonMsg(msg.wallet_deleted());
   }
      break;

   case WalletsMessage::kWalletAddresses: {
      std::vector<bs::sync::Address> addresses;
      for (const auto &addr : msg.wallet_addresses().addresses()) {
         for (const auto& hdWallet : hdWallets_) {
            try {
               const auto& assetType = hdWallet.second.leaves.at(addr.wallet_id());
               addresses.push_back({ std::move(bs::Address::fromAddressString(addr.address()))
                  , addr.index(), addr.wallet_id(), assetType });
               break;
            }
            catch (const std::exception&) {}
         }
      }
      processWalletAddresses(msg.wallet_addresses().wallet_id(), addresses);
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
         logger_->debug("ledger entries");
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

bs::message::ProcessingResult QtQuickAdapter::processHWW(const bs::message::Envelope& env)
{
   HW::DeviceMgrMessage msg;
   if (!msg.ParseFromString(env.message)) {
      logger_->error("[{}] failed to parse msg #{}", __func__, env.foreignId());
      return ProcessingResult::Error;
   }
   switch (msg.data_case()) {
   case HW::DeviceMgrMessage::kAvailableDevices:
      return processHWDevices(msg.available_devices());
   case HW::DeviceMgrMessage::kSignedTx:
      return processHWSignedTX(msg.signed_tx());
   case HW::DeviceMgrMessage::kRequestPin:
      curAuthDevice_ = bs::hww::fromMsg(msg.request_pin());
      QMetaObject::invokeMethod(this, [this] { emit invokePINentry(); });
      return bs::message::ProcessingResult::Success;
   case HW::DeviceMgrMessage::kPasswordRequest:
      curAuthDevice_ = bs::hww::fromMsg(msg.password_request().key());
      QMetaObject::invokeMethod(this, [this, onDevice=msg.password_request().allowed_on_device()]
         { emit invokePasswordEntry(QString::fromStdString(curAuthDevice_.label), onDevice); });
      return bs::message::ProcessingResult::Success;
   case HW::DeviceMgrMessage::kDeviceReady:
      return processHWWready(msg.device_ready());
   default: break;
   }
   return bs::message::ProcessingResult::Ignored;
}

void QtQuickAdapter::updateStates()
{
   //TODO
}

void QtQuickAdapter::setTopBlock(uint32_t topBlock)
{
   blockNum_ = topBlock;
   txModel_->setCurrentBlock(blockNum_);
   expTxByAddrModel_->setCurrentBlock(blockNum_);
   txInputsModel_->setTopBlock(topBlock);
}

void QtQuickAdapter::loadPlugins(QQmlApplicationEngine& engine)
{  // load embedded plugins
   pluginsListModel_->addPlugins({ new LeverexPlugin(this)
      , new SideshiftPlugin(logger_, engine, this)
      , new SideswapPlugin(this) });

   //TODO: send broadcast to request 3rd-party plugins loading
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

   setReq = msgReq->add_requests();
   setReq->set_source(SettingSource_Local);
   setReq->set_index(SetIdx_ExportDir);
   setReq->set_type(SettingType_String);

   setReq = msgReq->add_requests();
   setReq->set_source(SettingSource_Local);
   setReq->set_index(SetIdx_AddressFilterHideUsed);
   setReq->set_type(SettingType_Bool);

   setReq = msgReq->add_requests();
   setReq->set_source(SettingSource_Local);
   setReq->set_index(SetIdx_AddressFilterHideInternal);
   setReq->set_type(SettingType_Bool);

   setReq = msgReq->add_requests();
   setReq->set_source(SettingSource_Local);
   setReq->set_index(SetIdx_AddressFilterHideExternal);
   setReq->set_type(SettingType_Bool);

   setReq = msgReq->add_requests();
   setReq->set_source(SettingSource_Local);
   setReq->set_index(SetIdx_AddressFilterHideEmpty);
   setReq->set_type(SettingType_Bool);

   pushRequest(user_, userSettings_, msg.SerializeAsString());
}

void QtQuickAdapter::requestPostLoadingSettings()
{
   SettingsMessage msg;
   auto msgReq = msg.mutable_get_request();
   auto setReq = msgReq->add_requests();

   setReq = msgReq->add_requests();
   setReq->set_source(SettingSource_Local);
   setReq->set_index(SetIdx_TransactionFilterWalletName);
   setReq->set_type(SettingType_String);

   setReq = msgReq->add_requests();
   setReq->set_source(SettingSource_Local);
   setReq->set_index(SetIdx_TransactionFilterTransactionType);
   setReq->set_type(SettingType_String);

   setReq = msgReq->add_requests();
   setReq->set_source(SettingSource_Local);
   setReq->set_index(SetIdx_SelectedWallet);
   setReq->set_type(SettingType_Int);

   pushRequest(user_, userSettings_, msg.SerializeAsString());
}

void QtQuickAdapter::createWallet(bool primary)
{
   logger_->debug("[{}] primary: {}", __func__, primary);
}

std::string QtQuickAdapter::hdWalletIdByIndex(int index)
{
   const auto& walletsList = walletBalances_->wallets();
   if ((index < 0) || (index >= walletsList.size())) {
      return {};
   }
   const auto& walletName = walletsList.at(index).toStdString();
   for (const auto& wallet : hdWallets_) {
      if (wallet.second.name == walletName) {
         return wallet.first;
      }
   }
   return {};
}

std::string QtQuickAdapter::generateWalletName() const
{
   int index = walletBalances_->rowCount();
   std::string name;
   bool nameExists = true;
   while (nameExists) {
      name = "wallet" + std::to_string(++index);
      nameExists = false;
      for (const auto& w : walletNames_) {
         if (w.second == name) {
            nameExists = true;
            break;
         }
      }
   }
   return name;
}

void QtQuickAdapter::walletSelected(int index)
{
   if (index < 0 || index >= walletBalances_->wallets().size()) {
      addrModel_->reset("");
      txModel_->clear();
      return;
   }

   logger_->debug("[{}] {}", __func__, index);
   QMetaObject::invokeMethod(this, [this, index] {
      try {
         const auto& walletName = walletBalances_->wallets().at(index).toStdString();
         const auto& walletId = hdWalletIdByIndex(index);

         addrModel_->reset(walletId);
         WalletsMessage msg;
         msg.set_wallet_get(walletId);
         const auto msgId = pushRequest(user_, userWallets_, msg.SerializeAsString());
         walletInfoReq_[msgId] = walletName;

         if (hdWallets_.count(walletId) > 0) {
            const auto& hdWallet = hdWallets_.at(walletId);
            walletPropertiesModel_->setWalletInfo(QString::fromStdString(walletId), hdWallet);  
         }
         settingsController_->setParam(ApplicationSettings::Setting::SelectedWallet, index);
      }
      catch (const std::exception&) {}
   });
}

void QtQuickAdapter::processWalletLoaded(const bs::sync::WalletInfo &wi)
{
   const bool isInitialLoad = hdWallets_.empty();
   const auto& walletId = *wi.ids.cbegin();
   hdWallets_[walletId] = wi;
   logger_->debug("[QtQuickAdapter::processWalletLoaded] {} {}", wi.name, walletId);
   QMetaObject::invokeMethod(this, [this, isInitialLoad, walletId, walletName = wi.name] {
      hwDeviceModel_->setLoaded(walletId);
      walletBalances_->addWallet({ walletId, walletName });
      emit walletsListChanged();
   });

   WalletsMessage msg;
   msg.set_get_wallet_balances(walletId);
   pushRequest(user_, userWallets_, msg.SerializeAsString(), {}, 10
      , std::chrono::milliseconds{ 500 });
}

static QString assetTypeToString(const bs::AssetType assetType)
{
   switch (assetType) {
   case bs::AssetType::Legacy:   return QObject::tr("Legacy");
   case bs::AssetType::NestedSW: return QObject::tr("Nested SegWit");
   case bs::AssetType::NativeSW: return QObject::tr("Native SegWit");
   case bs::AssetType::Unknown:
   default: return QObject::tr("Unknown");
   }
}

ProcessingResult QtQuickAdapter::processWalletData(bs::message::SeqId msgId
   , const WalletsMessage_WalletData& response)
{
   walletPropertiesModel_->setNbUsedAddrs(response.wallet_id(), response.used_addresses_size());

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
   logger_->debug("[{}] {} used addresses", __func__, response.used_addresses_size());
   QVector<QVector<QString>> addresses;
   for (const auto& addr : response.used_addresses()) {
      try {
         addressCache_[bs::Address::fromAddressString(addr.address())] = response.wallet_id();
      }
      catch (const std::exception&) {}
      addresses.append({ QString::fromStdString(addr.address())
         , QString::fromStdString(addr.comment())
         , QString::fromStdString(addr.index())
         , assetTypeToString(static_cast<bs::AssetType>(addr.asset_type()))});
   }
   addrModel_->addRows(response.wallet_id(), addresses);
   return ProcessingResult::Success;
}

ProcessingResult QtQuickAdapter::processWalletBalances(bs::message::SeqId responseId
   , const WalletsMessage_WalletBalances &response)
{
   //logger_->debug("[{}] {}", __func__, response.DebugString());
   const WalletBalancesModel::Balance bal{ response.spendable_balance(), response.unconfirmed_balance()
      , response.total_balance(), response.nb_addresses() };
   walletBalances_->setWalletBalance(response.wallet_id(), bal);

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

      const auto &ownTxHash = BinaryData::fromString(resp.tx_hash());
      try {
         if (!resp.tx().empty()) {
            Tx tx(BinaryData::fromString(resp.tx()));
            if (tx.isInitialized()) {
               txDet.tx = std::move(tx);
               txDet.tx.setTxHeight(resp.tx_height());
               /*logger_->debug("[{}] own txid: {}/{}", ownTxHash.toHexStr(true)
                  , txDet.tx.getThisHash().toHexStr(true));*/
            }
         }
      } catch (const std::exception &e) {
         logger_->warn("[QtGuiAdapter::processTXDetails] TX deser error: {}", e.what());
      }
      for (const auto &addrStr : resp.out_addresses()) {
         try {
            txDet.outAddresses.push_back(std::move(bs::Address::fromAddressString(addrStr)));
         } catch (const std::exception &e) {
            logger_->warn("[QtGuiAdapter::processTXDetails] out deser '{}' error: {}"
               , addrStr, e.what());
         }
      }
      for (const auto &inAddr : resp.input_addresses()) {
         try {
            txDet.inputAddresses.push_back({ bs::Address::fromAddressString(inAddr.address())
               , inAddr.value(), inAddr.value_string(), inAddr.wallet_name()
               , static_cast<TXOUT_SCRIPT_TYPE>(inAddr.script_type())
               , BinaryData::fromString(inAddr.out_hash()), (uint32_t)inAddr.out_index() });
         } catch (const std::exception &e) {
            logger_->warn("[QtGuiAdapter::processTXDetails] input deser error: {}", e.what());
         }
      }
      for (const auto &outAddr : resp.output_addresses()) {
         try {
            txDet.outputAddresses.push_back({ bs::Address::fromAddressString(outAddr.address())
               , outAddr.value(), outAddr.value_string(), outAddr.wallet_name()
               , static_cast<TXOUT_SCRIPT_TYPE>(outAddr.script_type())
               , BinaryData::fromString(outAddr.out_hash()), (uint32_t)outAddr.out_index() });
         } catch (const std::exception &) { // OP_RETURN data for valueStr
            txDet.outputAddresses.push_back({ bs::Address{}
               , outAddr.value(), outAddr.address(), outAddr.wallet_name()
               , static_cast<TXOUT_SCRIPT_TYPE>(outAddr.script_type()), ownTxHash
               , (uint32_t)outAddr.out_index() });
         }
      }
      try {
         txDet.changeAddress = { bs::Address::fromAddressString(resp.change_address().address())
            , resp.change_address().value(), resp.change_address().value_string()
            , resp.change_address().wallet_name()
            , static_cast<TXOUT_SCRIPT_TYPE>(resp.change_address().script_type())
            , BinaryData::fromString(resp.change_address().out_hash())
            , (uint32_t)resp.change_address().out_index() };
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
         if (txDet.direction == bs::sync::Transaction::Direction::Revoke) {
            txModel_->removeTX(txDet.txHash);
         }
         else {
            txModel_->setDetails(txDet);
         }
      }
      else {
         QMetaObject::invokeMethod(this, [this, details = itTxDet->second, txDet] {
            details->setDetails(txDet);
            details->setCurBlock(blockNum_);
         }); // shouldn't be more than one entry
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
   txModel_->addRows(entries);
   pushRequest(user_, userWallets_, msg.SerializeAsString());
   return ProcessingResult::Success;
}

QStringList QtQuickAdapter::settingEnvironments() const
{
   return { tr("Main"), tr("Test") };
}

HwDeviceModel* QtQuickAdapter::devices()
{
    return nullptr;
}

bool QtQuickAdapter::scanningDevices() const
{
    return false;
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

QStringList QtQuickAdapter::completeBIP39dic(const QString& pfx)
{
   const auto& prefix = pfx.toLower().toStdString();
   if (prefix.empty()) {
      return {};
   }
   QStringList result;
   for (int i = 0; i < BIP39::NUM_BIP39_WORDS; ++i) {
      const auto& word = BIP39::get_word(i);
      bool prefixMatched = true;
      for (int j = 0; j < std::min(prefix.length(), word.length()); ++j) {
         if (word.at(j) != prefix.at(j)) {
            prefixMatched = false;
            break;
         }
      }
      if (prefixMatched) {
         result.append(QString::fromStdString(word));
      }
      if (result.size() >= 5) {
         break;
      }
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
   const auto walletName = name.isEmpty() ? generateWalletName() : name.toStdString();
   logger_->debug("[{}] {}", __func__, walletName);
   BIP39::word_list words;
   for (const auto& w : seed) {
      words.add(w.toStdString());
   }
   SignerMessage msg;
   auto msgReq = msg.mutable_create_wallet();
   msgReq->set_name(walletName);
   //msgReq->set_xpriv_key(seedData.xpriv);
   const auto binSeed = BIP39::seed_from_mnemonic(words);
   msgReq->set_seed(binSeed.toBinStr());
   msgReq->set_password(password.toStdString());
   pushRequest(user_, userSigner_, msg.SerializeAsString());
   walletBalances_->clear();
}

void QtQuickAdapter::importWallet(const QString& name, const QStringList& seed
   , const QString& password)
{
   const auto walletName = name.isEmpty() ? generateWalletName() : name.toStdString();
   logger_->debug("[{}] {}", __func__, walletName);
   BIP39::word_list words;
   for (const auto& w : seed) {
      words.add(w.toStdString());
   }
   SignerMessage msg;
   auto msgReq = msg.mutable_import_wallet();
   msgReq->set_name(walletName);
   //msgReq->set_description(seedData.description);
   //msgReq->set_xpriv_key(seedData.xpriv);
   const auto binSeed = BIP39::seed_from_mnemonic(words);
   msgReq->set_seed(binSeed.toBinStr());
   msgReq->set_password(password.toStdString());
   pushRequest(user_, userSigner_, msg.SerializeAsString());
   walletBalances_->clear();
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

QString QtQuickAdapter::pasteTextFromClipboard()
{
    return QGuiApplication::clipboard()->text();
}

bool QtQuickAdapter::validateAddress(const QString& addr)
{
   const auto& addrStr = addr.toStdString();
   try {
      bs::Address::fromAddressString(addrStr);
   }
   catch (const std::exception& e) {
      logger_->warn("[{}] invalid address {}: {}", __func__, addrStr, e.what());
      return false;
   }
   return true;
}

ArmoryServersModel* QtQuickAdapter::getArmoryServers()
{
   SettingsMessage msg;
   msg.mutable_armory_servers_get();
   const auto msgId = pushRequest(user_, userSettings_, msg.SerializeAsString());
   auto model = new ArmoryServersModel(logger_, this);
   armoryServersReq_[msgId] = model;
   connect(model, &ArmoryServersModel::changed, this, &QtQuickAdapter::onArmoryServerChanged);
   connect(model, &ArmoryServersModel::currentChanged, this, &QtQuickAdapter::onArmoryServerSelected);
   return model;
}

bool QtQuickAdapter::addArmoryServer(ArmoryServersModel* model, const QString& name
   , int netType, const QString& ipAddr, const QString& ipPort, const QString& key)
{
   for (const auto& srv : model->data()) {
      if (srv.name == name) {
         logger_->debug("[{}] armory server {} already exists", __func__, name.toStdString());
         return false;
      }
   }
   SettingsMessage msg;
   auto msgReq = msg.mutable_add_armory_server();
   msgReq->set_server_name(name.toStdString());
   msgReq->set_network_type(netType);
   msgReq->set_server_address(ipAddr.toStdString());
   msgReq->set_server_port(ipPort.toStdString());
   msgReq->set_server_key(key.toStdString());
   pushRequest(user_, userSettings_, msg.SerializeAsString());
   QMetaObject::invokeMethod(this, [model, name, netType, ipAddr, ipPort, key] {
      model->add({ name, static_cast<NetworkType>(netType), ipAddr, ipPort.toInt(), key });
   });
   return true;
}

bool QtQuickAdapter::delArmoryServer(ArmoryServersModel* model, int idx)
{
   logger_->debug("[{}] #{}", __func__, idx);
   if (!model->isEditable(idx)) {
      return false;
   }
   SettingsMessage msg;
   msg.set_del_armory_server(idx);
   pushRequest(user_, userSettings_, msg.SerializeAsString());
   model->del(idx);
   return true;
}

void QtQuickAdapter::onArmoryServerChanged(const QModelIndex& index, const QVariant& value)
{
   auto model = qobject_cast<ArmoryServersModel*>(sender());
   if (!model) {
      logger_->error("[{}] invalid sender", __func__);
      return;
   }
   auto srv = model->data(index.row());
   switch (index.column()) {
   case 0:
      for (int i = 0; i < model->rowCount(); ++i) {
         if (i == index.row()) {
            continue;
         }
         if (model->data(i).name == value.toString()) {
            return;
         }
      }
      srv.name = value.toString();
      break;
   case 1:
      srv.netType = static_cast<NetworkType>(value.toInt());
      break;
   case 2:
      srv.armoryDBIp = value.toString();
      break;
   case 3:
      srv.armoryDBPort = value.toInt();
      break;
   case 4:
      srv.armoryDBKey = value.toString();
      break;
   default: break;
   }
   SettingsMessage msg;
   auto msgReq = msg.mutable_upd_armory_server();
   msgReq->set_index(index.row());
   auto msgSrv = msgReq->mutable_server();
   msgSrv->set_server_name(srv.name.toStdString());
   msgSrv->set_server_address(srv.armoryDBIp.toStdString());
   msgSrv->set_server_port(std::to_string(srv.armoryDBPort));
   msgSrv->set_server_key(srv.armoryDBKey.toStdString());
   msgSrv->set_network_type((int)srv.netType);
   pushRequest(user_, userSettings_, msg.SerializeAsString());
}

void QtQuickAdapter::requestFeeSuggestions()
{
   ArmoryMessage msg;
   auto msgReq = msg.mutable_fee_levels_request();
   for (const auto& feeLevel : FeeSuggestionModel::feeLevels()) {
      msgReq->add_levels(feeLevel.first);
   }
   pushRequest(user_, userBlockchain_, msg.SerializeAsString(), {}, 10
      , std::chrono::milliseconds{500});
   feeSuggModel_->clear();
}

QTXSignRequest* QtQuickAdapter::createTXSignRequest(int walletIndex, const QStringList& recvAddrs
   , const QList<double>& recvAmounts, double fee, const QString& comment, bool isRbf, QUTXOList* utxos)
{
   WalletsMessage msg;
   auto msgReq = msg.mutable_tx_request();
   if (walletIndex >= 0) {
      msgReq->set_hd_wallet_id(hdWalletIdByIndex(walletIndex));
   }
   bool isMaxAmount = false;
   if (recvAddrs.isEmpty()) {
      const auto& recipients = txOutputsModel_->recipients();
      for (const auto& recip : recipients) {
         try {
            const auto& addr = bs::Address::fromRecipient(recip);
            auto msgOut = msgReq->add_outputs();
            msgOut->set_address(addr.display());
            if (recip->getValue()) {
               msgOut->set_amount(recip->getValue() / BTCNumericTypes::BalanceDivider);
            }
            else {
               isMaxAmount = true;
            }
         }
         catch (const std::exception& e) {
            logger_->error("[{}] recipient {}", __func__, e.what());
         }
      }
   }
   else {
      int idx = 0;
      for (const auto& recvAddr : recvAddrs) {
         try {
            const auto& addr = bs::Address::fromAddressString(recvAddr.toStdString());
            auto msgOut = msgReq->add_outputs();
            msgOut->set_address(addr.display());
            if (recvAmounts.size() > idx) {
               msgOut->set_amount(recvAmounts.at(idx));
            }
            else {
               isMaxAmount = true;
            }
         }
         catch (const std::exception& e) {
            logger_->error("[{}] recvAddr {}", __func__, e.what());
         }
         idx++;
      }
   }
   msgReq->set_rbf(isRbf);
   msgReq->set_fee_per_byte(fee);
   if (!comment.isEmpty()) {
      msgReq->set_comment(comment.toStdString());
   }
   QTXSignRequest* txReq = nullptr;
   std::vector<UTXO> inputs;
   if (utxos) {
      if (walletIndex < 0) {
         txReq = new QTXSignRequest(logger_, this);
         for (const auto& qUtxo : utxos->data()) {
            txReq->addInput(qUtxo->input());
         }
         ArmoryMessage msgSpendable;
         ArmoryMessage_WalletIDs* msgWltIds = nullptr;
         if (walletIndex == -1) {      // RBF mode
            msgWltIds = msgSpendable.mutable_get_rbf_utxos();
         }
         else if (walletIndex == -2) { // CPFP mode
            msgWltIds = msgSpendable.mutable_get_zc_utxos();
         }
         if (msgWltIds) {
            for (const auto& wallet : hdWallets_) {
               for (const auto& leaf : wallet.second.leaves) {
                  auto leafId = leaf.first;
                  msgWltIds->add_wallet_ids(leafId);
                  for (auto& c : leafId) {
                     c = std::toupper(c);
                  }
                  msgWltIds->add_wallet_ids(leafId);
               }
            }
            const auto msgId = pushRequest(user_, userBlockchain_, msgSpendable.SerializeAsString());
            txReqs_[msgId] = { txReq, isMaxAmount, msg };
            logger_->debug("[{}] {}", __func__, msgSpendable.DebugString());
            return txReq;
         }
         else {
            emit showError(tr("Unknown TX mode"));
            return txReq;
         }
      }
      else {
         for (const auto& qUtxo : utxos->data()) {
            msgReq->add_utxos(qUtxo->utxo().serialize().toBinStr());
            inputs.push_back(qUtxo->utxo());
         }
      }
   }
   const auto msgId = pushRequest(user_, userWallets_, msg.SerializeAsString());
   if (!txReq) {
      txReq = new QTXSignRequest(logger_, this);
   }
   if (!inputs.empty()) {
      txReq->setInputs(inputs);
   }
   txReqs_[msgId] = { txReq, isMaxAmount };
   return txReq;
}

void QtQuickAdapter::getUTXOsForWallet(int walletIndex)
{
   txInputsModel_->clear();
   WalletsMessage msg;
   auto msgReq = msg.mutable_get_utxos();
   msgReq->set_wallet_id(hdWalletIdByIndex(walletIndex));
   msgReq->set_confirmed_only(false);
   pushRequest(user_, userWallets_, msg.SerializeAsString());
}

QTxDetails* QtQuickAdapter::getTXDetails(const QString& txHash)
{
   auto txBinHash = BinaryData::CreateFromHex(txHash.trimmed().toStdString());
   txBinHash.swapEndian();
   if (txBinHash.getSize() != 32) {
      logger_->warn("[{}] invalid TX hash size {}", __func__, txBinHash.getSize());
      return new QTxDetails(logger_, {}, this);
   }
   WalletsMessage msg;
   auto msgReq = msg.mutable_tx_details_request();
   auto txReq = msgReq->add_requests();
   txReq->set_tx_hash(txBinHash.toBinStr());
   const auto msgId = pushRequest(user_, userWallets_, msg.SerializeAsString());
   const auto txDet = new QTxDetails(logger_, txBinHash, this);
   txDetailReqs_[msgId] = txDet;
   return txDet;
}

void QtQuickAdapter::pollHWWallets()
{
   hwDevicesPolling_ = true;
   HW::DeviceMgrMessage msg;
   msg.mutable_startscan();
   pushRequest(user_, userHWW_, msg.SerializeAsString());
}

void QtQuickAdapter::stopHWWalletsPolling()
{
   hwDevicesPolling_ = false;
}

void QtQuickAdapter::setHWpin(const QString& pin)
{
   if (curAuthDevice_.id.empty()) {
      logger_->error("[{}] no device requested PIN", __func__);
      return;
   }
   HW::DeviceMgrMessage msg;
   auto msgReq = msg.mutable_set_pin();
   bs::hww::deviceKeyToMsg(curAuthDevice_, msgReq->mutable_key());
   msgReq->set_pin(pin.toStdString());
   pushRequest(user_, userHWW_, msg.SerializeAsString());
   curAuthDevice_ = {};
}

void QtQuickAdapter::setHWpassword(const QString& password)
{
   if (curAuthDevice_.id.empty()) {
      logger_->error("[{}] no device requested passphrase", __func__);
      return;
   }
   HW::DeviceMgrMessage msg;
   auto msgReq = msg.mutable_set_password();
   bs::hww::deviceKeyToMsg(curAuthDevice_, msgReq->mutable_key());
   if (password.isEmpty()) {
      msgReq->set_set_on_device(true);
   }
   else {
      msgReq->set_password(password.toStdString());
   }
   pushRequest(user_, userHWW_, msg.SerializeAsString());
   curAuthDevice_ = {};
}

void QtQuickAdapter::importWOWallet(const QString& filename)
{
   if (filename.isEmpty() || !SystemFileUtils::fileExist(filename.toStdString())) {
      emit showError(tr("Invalid or non-existing wallet file %1").arg(filename));
      return;
   }
   logger_->debug("[{}] {}", __func__, filename.toStdString());
   SignerMessage msg;
   msg.set_import_wo_wallet(filename.toStdString());
   pushRequest(user_, userSigner_, msg.SerializeAsString());
}

void QtQuickAdapter::importHWWallet(int deviceIndex)
{
   auto devKey = hwDeviceModel_->getDevice(deviceIndex);
   if (devKey.id.empty() && (deviceIndex >= 0)) {
      emit showError(tr("Invalid device #%1").arg(deviceIndex));
      return;
   }
   logger_->debug("[{}] {}", __func__, deviceIndex);
   HW::DeviceMgrMessage msg;
   bs::hww::deviceKeyToMsg(devKey, msg.mutable_import_device());
   pushRequest(user_, userHWW_, msg.SerializeAsString());
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
   const auto msgId = pushRequest(user_, userBlockchain_, msg.SerializeAsString()
      , {}, 3, std::chrono::milliseconds{2300} );
   expTxAddrReqs_.insert(msgId);
   logger_->debug("[{}] response handling done", __func__);
   return ProcessingResult::Success;
}

ProcessingResult QtQuickAdapter::processFeeLevels(const ArmoryMessage_FeeLevelsResponse& response)
{
   std::map<uint32_t, float> feeLevels;
   for (const auto& pair : response.fee_levels()) {
      feeLevels[pair.level()] = pair.fee();
   }
   feeSuggModel_->addRows(feeLevels);
   return ProcessingResult::Success;
}

ProcessingResult QtQuickAdapter::processWalletsList(const WalletsMessage_WalletsListResponse& response)
{
   logger_->debug("[QtQuickAdapter::processWalletsList] {}", response.DebugString());
   QMetaObject::invokeMethod(this, [this, response] {
      walletBalances_->clear();
      for (const auto& wallet : response.wallets()) {
         const auto& hdWallet = bs::sync::HDWalletData::fromCommonMessage(wallet);
         walletBalances_->addWallet({hdWallet.id, hdWallet.name});
      }
      emit walletsListChanged();
   });
   return ProcessingResult::Success;
}

bs::message::ProcessingResult QtQuickAdapter::processWalletDeleted(const std::string& walletId)
{
   if (walletId.empty()) {
      emit failedDeleteWallet();
      return bs::message::ProcessingResult::Ignored;
   }
   logger_->debug("[{}] {}", __func__, walletId);

   walletBalances_->deleteWallet(walletId);
   emit successDeleteWallet();
   return bs::message::ProcessingResult::Success;
}

bs::message::ProcessingResult QtQuickAdapter::processWalletSeed(const BlockSettle::Common::SignerMessage_WalletSeed& response)
{
   if (response.bip39_seed().empty()) {
      emit walletSeedAuthFailed(tr("Failed to obtain wallet seed"));
      return bs::message::ProcessingResult::Error;
   }
   walletPropertiesModel_->setWalletSeed(response.wallet_id(), response.bip39_seed());
   emit walletSeedAuthSuccess();
   return bs::message::ProcessingResult::Success;
}

ProcessingResult QtQuickAdapter::processUTXOs(const WalletsMessage_UtxoListResponse& response)
{
   logger_->debug("[{}] {} UTXOs for {}", __func__, response.utxos_size(), response.wallet_id());
   walletPropertiesModel_->setNbUTXOs(response.wallet_id(), response.utxos_size());

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
   if (!response.signed_tx().empty()) {
      const auto& signedTX = BinaryData::fromString(response.signed_tx());
      logger_->debug("[{}] signed TX size: {}", __func__, signedTX.getSize());
      ArmoryMessage msg;
      auto msgReq = msg.mutable_tx_push();
      //msgReq->set_push_id(id);
      auto msgTx = msgReq->add_txs_to_push();
      msgTx->set_tx(response.signed_tx());
      //not adding TX hashes atm
      pushRequest(user_, userBlockchain_, msg.SerializeAsString());
      emit successTx();
   }
   else {
      emit failedTx(tr("TX sign failed\nerror %1: %2").arg(response.error_code())
         .arg(QString::fromStdString(response.error_text())));
   }
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
      QMetaObject::invokeMethod(this, [this, txEntry] { notifyNewTransaction(txEntry); });
      
   }
   pushRequest(user_, userWallets_, msg.SerializeAsString());
   return ProcessingResult::Success;
}

ProcessingResult QtQuickAdapter::processZCInvalidated(const ArmoryMessage_ZCInvalidated& zcInv)
{
   for (const auto& hashStr : zcInv.tx_hashes()) {
      const auto& txHash = BinaryData::fromString(hashStr);
      WalletsMessage msg;
      auto msgReq = msg.mutable_tx_details_request();
      auto txReq = msgReq->add_requests();
      txReq->set_tx_hash(txHash.toBinStr());
      pushRequest(user_, userWallets_, msg.SerializeAsString());
   }
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
      const auto msgIdReq = pushRequest(user_, userBlockchain_, msg.SerializeAsString()
         , {}, 3, std::chrono::milliseconds{ 2300 });
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

void QtQuickAdapter::processWalletAddresses(const std::string& walletId
   , const std::vector<bs::sync::Address>& addresses)
{
   if (addresses.empty()) {
      logger_->debug("[{}] {} no addresses", __func__, walletId);
      return;
   }
   auto hdWalletId = walletId;
   for (const auto& hdWallet : hdWallets_) {
      if (hdWallet.second.hasLeaf(walletId)) {
         hdWalletId = hdWallet.first;
         break;
      }
   }
   const auto lastAddr = addresses.at(addresses.size() - 1);
   logger_->debug("[{}] {} last address: {}", __func__, hdWalletId, lastAddr.address.display());
   addressCache_[lastAddr.address] = hdWalletId;
   addrModel_->addRow(hdWalletId, { QString::fromStdString(lastAddr.address.display())
      , QString(), QString::fromStdString(lastAddr.index), assetTypeToString(lastAddr.assetType)});
   generatedAddress_ = lastAddr.address;
   emit addressGenerated();
}

bs::message::ProcessingResult QtQuickAdapter::processTxResponse(bs::message::SeqId msgId
   , const WalletsMessage_TxResponse& response)
{
   logger_->debug("[{}] {}", __func__, response.DebugString());
   const auto& itReq = txReqs_.find(msgId);
   auto txReq = bs::signer::pbTxRequestToCore(response.tx_sign_request(), logger_);
   if (itReq == txReqs_.end()) {
      if (txSaveReqs_.empty()) {
         logger_->error("[{}] unknown request #{}", __func__, msgId);
         return bs::message::ProcessingResult::Error;
      }
      const auto exportPath = *txSaveReqs_.rbegin();
      txSaveReqs_.pop_back();
      saveTransaction(txReq, exportPath);
      return bs::message::ProcessingResult::Success;
   }
   auto qReq = itReq->second.txReq;
   const bool noReqAmount = itReq->second.isMaxAmount;
   txReqs_.erase(itReq);
   if (!response.error_text().empty()) {
      logger_->error("[{}] {}", __func__, response.error_text());
      qReq->setError(QString::fromStdString(response.error_text()));
      return bs::message::ProcessingResult::Success;
   }

   std::unordered_set<std::string> hdWalletIds;
   for (const auto& walletId : txReq.walletIds) {
      for (const auto& hdWallet : hdWallets_) {
         if (hdWallet.second.hasLeaf(walletId)) {
            hdWalletIds.insert(hdWallet.first);
         }
      }
   }
   logger_->debug("[{}] {} HD walletId[s]", __func__, hdWalletIds.size());
   txReq.walletIds.clear();
   for (const auto& walletId : hdWalletIds) {
      txReq.walletIds.push_back(walletId);
      try {
         const auto wallet = hdWallets_.at(walletId);
         if (wallet.isHardware) {
            qReq->setHWW(true);
            logger_->debug("[{}] noReqAmt: {} for {}", __func__, noReqAmount, walletId);
            if (!noReqAmount) {
               HW::DeviceMgrMessage msg;
               msg.set_prepare_wallet_for_tx_sign(walletId);
               pushRequest(user_, userHWW_, msg.SerializeAsString());
               hwwReady_[walletId] = qReq;
            }
         }
         else if (wallet.watchOnly) {
            qReq->setWatchingOnly(true);
         }
      }
      catch (const std::exception&) {
         logger_->error("[{}] unknown walletId {}", __func__, walletId);
      }
   }
   qReq->setTxSignReq(txReq);
   return bs::message::ProcessingResult::Success;
}

std::string QtQuickAdapter::hdWalletIdByLeafId(const std::string& walletId) const
{
   for (const auto& hdWallet : hdWallets_) {
      for (const auto& leaf : hdWallet.second.leaves) {
         if (walletId == leaf.first) {
            return hdWallet.first;
         }
      }
   }
   return {};
}

bs::message::ProcessingResult QtQuickAdapter::processUTXOs(bs::message::SeqId msgId
   , const ArmoryMessage_UTXOs& response)
{
   const auto& itReq = txReqs_.find(msgId);
   if (itReq == txReqs_.end()) {
      logger_->error("[{}] unknown request #{}", __func__, msgId);
      return bs::message::ProcessingResult::Error;
   }
   std::vector<UTXO> utxos;
   utxos.reserve(response.utxos_size());
   try {
      for (const auto& serUtxo : response.utxos()) {
         UTXO utxo;
         utxo.unserialize(BinaryData::fromString(serUtxo));
         utxos.emplace_back(std::move(utxo));
      }
   }
   catch (const std::exception& e) {
      logger_->error("[{}] failed to deser UTXO: {}", __func__, e.what());
   }
   const auto& dummyUTXOs = itReq->second.txReq->inputs();
   auto msgReq = itReq->second.msg.mutable_tx_request();
   std::vector<UTXO> inputs;
   std::set<bs::Address> inputAddrs;
   for (const auto& u : dummyUTXOs) {
      for (const auto& utxo : utxos) {
         logger_->debug("[{}] {}:{} vs {}:{}", __func__, u.txHash.toHexStr(true), u.txOutIndex
            , utxo.getTxHash().toHexStr(true), utxo.getTxOutIndex());
         if ((u.txHash == utxo.getTxHash()) && (u.txOutIndex == utxo.getTxOutIndex())) {
            try {
               inputAddrs.insert(bs::Address::fromUTXO(utxo));
            }
            catch (const std::exception&) {}
            msgReq->add_utxos(utxo.serialize().toBinStr());
            inputs.push_back(utxo);
            break;
         }
      }
   }
   itReq->second.txReq->setInputs(inputs);
   logger_->debug("[{}] matched {} UTXOs of {}", __func__, msgReq->utxos_size(), dummyUTXOs.size());
   if (msgReq->utxos_size() != dummyUTXOs.size()) {
      itReq->second.txReq->setError(tr("Failed to obtain UTXO[s]"));
      txReqs_.erase(itReq);
      return bs::message::ProcessingResult::Error;
   }
   for (const auto& addr : inputAddrs) {
      const auto& itAddr = addressCache_.find(addr);
      if (itAddr != addressCache_.end()) {
         msgReq->set_hd_wallet_id(itAddr->second);
         logger_->debug("[{}] set HD wallet id {} from {}", __func__, msgReq->hd_wallet_id(), addr.display());
         break;
      }
   }
   msgId = pushRequest(user_, userWallets_, itReq->second.msg.SerializeAsString());
   txReqs_[msgId] = { itReq->second.txReq, itReq->second.isMaxAmount };
   txReqs_.erase(itReq);
   return bs::message::ProcessingResult::Success;
}

bs::message::ProcessingResult QtQuickAdapter::processHWDevices(const HW::DeviceMgrMessage_Devices& response)
{
   std::vector<bs::hww::DeviceKey> devices;
   for (const auto& key : response.device_keys()) {
      devices.push_back(bs::hww::fromMsg(key));
   }
   hwDeviceModel_->setDevices(devices);
   if (devices.empty() && hwDevicesPolling_) {
      HW::DeviceMgrMessage msg;
      msg.mutable_startscan();
      pushRequest(user_, userHWW_, msg.SerializeAsString()
         , bs::message::bus_clock::now() + std::chrono::seconds{ 1 });
   }
   else {
      hwDevicesPolling_ = false;
      for (const auto& hdWallet : hdWallets_) {
         hwDeviceModel_->setLoaded(hdWallet.first);
      }
      hwDeviceModel_->findNewDevice();
   }
   return bs::message::ProcessingResult::Success;
}

bs::message::ProcessingResult QtQuickAdapter::processHWWready(const std::string& walletId)
{
   logger_->debug("[{}] wallet {}", __func__, walletId);
   const auto& it = hwwReady_.find(walletId);
   if (it == hwwReady_.end()) {
      return bs::message::ProcessingResult::Ignored;
   }
   it->second->setHWWready();
   hwwReady_.erase(it);
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

bs::message::ProcessingResult QtQuickAdapter::processHWSignedTX(const HW::DeviceMgrMessage_SignTxResponse& response)
{
   if (!response.signed_tx().empty()) {
      ArmoryMessage msg;
      auto msgReq = msg.mutable_tx_push();
      auto msgTx = msgReq->add_txs_to_push();
      msgTx->set_tx(response.signed_tx());
      pushRequest(user_, userBlockchain_, msg.SerializeAsString());
   }
   else {
      emit showError(QString::fromStdString(response.error_msg()));
   }
   return ProcessingResult::Success;
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

void QtQuickAdapter::signAndBroadcast(QTXSignRequest* txReq, const QString& password)
{
   if (!txReq) {
      logger_->error("[{}] no TX request passed", __func__);
      return;
   }
   auto txSignReq = txReq->txReq();
   logger_->debug("[{}] HW sign: {}", __func__, txReq->isHWW());
   if (txReq->isHWW()) {
      HW::DeviceMgrMessage msg;
      *msg.mutable_sign_tx() = bs::signer::coreTxRequestToPb(txSignReq);
      pushRequest(user_, userHWW_, msg.SerializeAsString());
   }
   else {
      SignerMessage msg;
      auto msgReq = msg.mutable_sign_tx_request();
      //msgReq->set_id(id);
      *msgReq->mutable_tx_request() = bs::signer::coreTxRequestToPb(txSignReq);
      msgReq->set_sign_mode((int)SignContainer::TXSignMode::Full);
      //msgReq->set_keep_dup_recips(keepDupRecips);
      msgReq->set_passphrase(password.toStdString());
      pushRequest(user_, userSigner_, msg.SerializeAsString());
   }
}

int QtQuickAdapter::getSearchInputType(const QString& s)
{
   const auto& trimmed = s.trimmed().toStdString();
   if (validateAddress(s)) {
      return 1;
   }

   if (trimmed.length() == 64) { // potential TX hash in hex
      const auto& txId = BinaryData::CreateFromHex(trimmed);
      if (txId.getSize() == 32) {   // valid TXid
         return 2;
      }
   }
   return 0;
}

void QtQuickAdapter::startAddressSearch(const QString& s)
{
   expTxByAddrModel_->clear();
   ArmoryMessage msg;
   msg.set_get_address_history(s.trimmed().toStdString());
   const auto msgId = pushRequest(user_, userBlockchain_, msg.SerializeAsString()
      , {}, 1, std::chrono::seconds{ 15 });
   logger_->debug("[{}] #{}", __func__, msgId);
}

qtquick_gui::WalletPropertiesVM* QtQuickAdapter::walletProperitesVM() const
{
   return walletPropertiesModel_.get();
}

int QtQuickAdapter::rescanWallet(const QString& walletId)
{
   logger_->debug("[{}] {}", __func__, walletId.toStdString());
   WalletsMessage msg;
   msg.set_wallet_rescan(walletId.toStdString());
   const auto msgId = pushRequest(user_, userWallets_, msg.SerializeAsString());
   return (msgId == 0) ? -1 : 0;
}

int QtQuickAdapter::renameWallet(const QString& walletId, const QString& newName)
{
   logger_->debug("[{}] {} -> {}", __func__, walletId.toStdString(), newName.toStdString());
   const auto& itWallet = hdWallets_.find(walletId.toStdString());
   if (itWallet == hdWallets_.end()) {
      showError(tr("Wallet %1 not found").arg(walletId));
      return -1;
   }
   itWallet->second.name = newName.toStdString();
   SignerMessage msg;
   auto msgReq = msg.mutable_set_wallet_name();
   msgReq->mutable_wallet()->set_wallet_id(walletId.toStdString());
   msgReq->set_new_name(newName.toStdString());
   pushRequest(user_, userSigner_, msg.SerializeAsString());
   walletBalances_->rename(walletId.toStdString(), newName.toStdString());
   walletPropertiesModel_->rename(walletId.toStdString(), newName.toStdString());
   return 0;
}

int QtQuickAdapter::changePassword(const QString& walletId, const QString& oldPassword, const QString& newPassword)
{
   SignerMessage msg;
   auto msgReq = msg.mutable_change_wallet_pass();
   auto msgWallet = msgReq->mutable_wallet();
   msgWallet->set_wallet_id(walletId.toStdString());
   msgWallet->set_password(oldPassword.toStdString());
   msgReq->set_new_password(newPassword.toStdString());
   const auto msgId = pushRequest(user_, userSigner_, msg.SerializeAsString());
   return (msgId == 0) ? -1 : 0;
}

bool QtQuickAdapter::isWalletPasswordValid(const QString& walletId, const QString& Password)
{
   if (Password.isEmpty()) {
      return false;
   }

   return true;
}

bool QtQuickAdapter::isWalletNameExist(const QString& walletName)
{
   return walletBalances_->nameExist(walletName.toStdString());
}

bool QtQuickAdapter::verifyPasswordIntegrity(const QString& password)
{
   return password.length() >= kMinPasswordLength;
}

int QtQuickAdapter::exportWallet(const QString& walletId, const QString& exportDir)
{
   SignerMessage msg;
   auto msgReq = msg.mutable_export_wo_wallet_request();
   msgReq->mutable_wallet()->set_wallet_id(walletId.toStdString());
   msgReq->set_output_dir(exportDir.toStdString());
   const auto msgId = pushRequest(user_, userSigner_, msg.SerializeAsString());
   return (msgId == 0) ? -1 : 0;
}

int QtQuickAdapter::viewWalletSeedAuth(const QString& walletId, const QString& password)
{
   SignerMessage msg;
   auto msgReq = msg.mutable_get_wallet_seed();
   msgReq->set_wallet_id(walletId.toStdString());
   msgReq->set_password(password.toStdString());
   const auto msgId = pushRequest(user_, userSigner_, msg.SerializeAsString());
   return (msgId == 0) ? -1 : 0;
}

int QtQuickAdapter::deleteWallet(const QString& walletId, const QString& password)
{
   SignerMessage msg;
   auto msgReq = msg.mutable_delete_wallet();
   msgReq->set_wallet_id(walletId.toStdString());
   msgReq->set_password(password.toStdString());
   const auto msgId = pushRequest(user_, userSigner_, msg.SerializeAsString());

   return (msgId == 0) ? -1 : 0;
}

void QtQuickAdapter::notifyNewTransaction(const bs::TXEntry& tx)
{
   const auto txId = QString::fromStdString(tx.txHash.toHexStr(true));
   auto txDetails = getTXDetails(txId);
   connect(txDetails, &QTxDetails::updated, this, [txDetails, tx, this](){
      logger_->debug("[QtQuickAdapter::notifyNewTransaction] {}: {}", txDetails->txId().toStdString(), txDetails->timestamp().toStdString());
      showNotification(
         tr("BlockSettle Terminal"),
         tr("%1: %2\n%3: %4\n%5: %6\n%7: %8\n").arg(
              tr("Date"), QDateTime::fromSecsSinceEpoch(tx.txTime).toString(gui_utils::dateTimeFormat)
            , tr("Amount"), gui_utils::satoshiToQString(std::abs(tx.value))
            , tr("Type"), gui_utils::directionToQString(txDetails->direction())
            , tr("Address"), !tx.addresses.empty() ? QString::fromStdString((*tx.addresses.cbegin()).display()) : tr("")
         )
      );
      txDetails->disconnect(this);
   }, Qt::QueuedConnection);
}

void QtQuickAdapter::exportTransaction(const QUrl& path, QTXSignRequest* request)
{
   txSaveReqs_.push_back(path.toLocalFile().toStdString());
}

void QtQuickAdapter::saveTransaction(const bs::core::wallet::TXSignRequest& txReq, const std::string& exportPath)
{
   if (txReq.walletIds.empty()) {
      emit transactionExportFailed(tr("TX request doesn't contain wallets"));
      return;
   }
   const auto& timestamp = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
   auto walletId = *txReq.walletIds.cbegin();
   for (const auto& hdWallet : hdWallets_) {
      for (const auto& leaf : hdWallet.second.leaves) {
         if (leaf.first == walletId) {
            walletId = hdWallet.first;
            break;
         }
      }
   }
   const std::string filename = "BlockSettle_" + walletId + "_" + std::to_string(timestamp) + "_unsigned.bin";
   const auto& pathName = exportPath + "/" + filename;
   logger_->debug("[{}] exporting transaction to {}", __func__, pathName);
   const auto& txSer = bs::signer::coreTxRequestToPb(txReq, false).SerializeAsString();

   auto f = fopen(pathName.c_str(), "wb");
   if (!f) {
      emit transactionExportFailed(tr("Failed to open %1 for writing").arg(QString::fromStdString(pathName)));
      return;
   }
   if (fwrite(txSer.data(), 1, txSer.size(), f) != txSer.size()) {
      logger_->error("[{}] failed to write {} bytes to {}", __func__, txSer.size(), pathName);
      emit transactionExportFailed(tr("Failed to write %1 bytes to %2. Disk full?")
         .arg(txSer.size()).arg(QString::fromStdString(pathName)));
      return;
   }
   fclose(f);
   logger_->debug("[{}] exporting {} done", __func__, pathName);
   emit transactionExported(QString::fromStdString(pathName));
}

QTXSignRequest* QtQuickAdapter::importTransaction(const QUrl& path)
{
   const auto& pathName = path.toLocalFile().toStdString();
   auto f = fopen(pathName.c_str(), "rb");
   if (!f) {
      emit transactionImportFailed(tr("Failed to open %1 for reading")
         .arg(QString::fromStdString(pathName)));
      return nullptr;
   }
   std::string txSer;
   char buf[512];
   size_t rc = 0;
   while ((rc = fread(buf, 1, sizeof(buf), f)) > 0) {
      txSer.append(std::string(buf, rc));
   }
   if (txSer.empty()) {
      emit transactionImportFailed(tr("Failed to read from %1")
         .arg(QString::fromStdString(pathName)));
      return nullptr;
   }
   Blocksettle::Communication::headless::SignTxRequest msg;
   if (!msg.ParseFromString(txSer)) {
      emit transactionImportFailed(tr("Failed to parse %1 bytes from %2")
         .arg(txSer.size()).arg(QString::fromStdString(pathName)));
      return nullptr;
   }
   const auto& txReq = bs::signer::pbTxRequestToCore(msg);
   if (!txReq.isValid()) {
      emit transactionImportFailed(tr("Failed to obtain valid data from %1")
         .arg(QString::fromStdString(pathName)));
      return nullptr;
   }
   auto txSignRequest = new QTXSignRequest(logger_, this);
   txSignRequest->setTxSignReq(txReq);
   return txSignRequest;
}
