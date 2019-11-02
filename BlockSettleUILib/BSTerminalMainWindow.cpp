#include "BSTerminalMainWindow.h"

#include <QApplication>
#include <QCloseEvent>
#include <QGuiApplication>
#include <QIcon>
#include <QShortcut>
#include <QStringList>
#include <QSystemTrayIcon>
#include <QToolBar>
#include <QTreeView>
#include <spdlog/spdlog.h>
#include <thread>

#include "AboutDialog.h"
#include "ArmoryServersProvider.h"
#include "AssetManager.h"
#include "AuthAddressDialog.h"
#include "AuthAddressManager.h"
#include "AutheIDClient.h"
#include "BSMarketDataProvider.h"
#include "BSMessageBox.h"
#include "BSTerminalSplashScreen.h"
#include "CCFileManager.h"
#include "CCPortfolioModel.h"
#include "CCTokenEntryDialog.h"
#include "CelerAccountInfoDialog.h"
#include "ConnectionManager.h"
#include "CreateTransactionDialogAdvanced.h"
#include "CreateTransactionDialogSimple.h"
#include "DialogManager.h"
#include "FutureValue.h"
#include "HeadlessContainer.h"
#include "ImportKeyBox.h"
#include "LoginWindow.h"
#include "MDAgreementDialog.h"
#include "MarketDataProvider.h"
#include "NetworkSettingsLoader.h"
#include "NewAddressDialog.h"
#include "NewWalletDialog.h"
#include "NotificationCenter.h"
#include "OrderListModel.h"
#include "PubKeyLoader.h"
#include "QuoteProvider.h"
#include "RequestReplyCommand.h"
#include "SelectWalletDialog.h"
#include "Settings/ConfigDialog.h"
#include "SignersProvider.h"
#include "StartupDialog.h"
#include "StatusBarView.h"
#include "SystemFileUtils.h"
#include "TabWithShortcut.h"
#include "TerminalEncryptionDialog.h"
#include "TransactionsViewModel.h"
#include "TransactionsWidget.h"
#include "UiUtils.h"
#include "UtxoReserveAdapters.h"
#include "Wallets/SyncHDWallet.h"
#include "Wallets/SyncWalletsManager.h"

#include "ui_BSTerminalMainWindow.h"

BSTerminalMainWindow::BSTerminalMainWindow(const std::shared_ptr<ApplicationSettings>& settings
   , BSTerminalSplashScreen& splashScreen, QWidget* parent)
   : QMainWindow(parent)
   , ui_(new Ui::BSTerminalMainWindow())
   , applicationSettings_(settings)
{
   bs::UtxoReservation::init();

   UiUtils::SetupLocale();

   ui_->setupUi(this);

   setupShortcuts();

   loginButtonText_ = tr("Login");

   armoryServersProvider_= std::make_shared<ArmoryServersProvider>(applicationSettings_);
   signersProvider_= std::make_shared<SignersProvider>(applicationSettings_);

   bool licenseAccepted = showStartupDialog();
   if (!licenseAccepted) {
      QMetaObject::invokeMethod(this, []() {
         qApp->exit(EXIT_FAILURE);
      }, Qt::QueuedConnection);
      return;
   }

   auto geom = settings->get<QRect>(ApplicationSettings::GUI_main_geometry);
   if (!geom.isEmpty()) {
      setGeometry(geom);
   }

   connect(ui_->actionQuit, &QAction::triggered, qApp, &QCoreApplication::quit);

   logMgr_ = std::make_shared<bs::LogManager>();
   logMgr_->add(applicationSettings_->GetLogsConfig());

   logMgr_->logger()->debug("Settings loaded from {}", applicationSettings_->GetSettingsPath().toStdString());

   setupIcon();
   UiUtils::setupIconFont(this);
   NotificationCenter::createInstance(logMgr_->logger(), applicationSettings_, ui_.get(), sysTrayIcon_, this);

   cbApprovePuB_ = PubKeyLoader::getApprovingCallback(PubKeyLoader::KeyType::PublicBridge
      , this, applicationSettings_);
   cbApproveChat_ = PubKeyLoader::getApprovingCallback(PubKeyLoader::KeyType::Chat
      , this, applicationSettings_);
   cbApproveProxy_ = PubKeyLoader::getApprovingCallback(PubKeyLoader::KeyType::Proxy
      , this, applicationSettings_);

   initConnections();
   initArmory();

   walletsMgr_ = std::make_shared<bs::sync::WalletsManager>(logMgr_->logger(), applicationSettings_, armory_);
   dealerUtxoAdapter_ = std::make_shared<bs::DealerUtxoResAdapter>(logMgr_->logger(), nullptr);

   if (!applicationSettings_->get<bool>(ApplicationSettings::initialized)) {
      applicationSettings_->SetDefaultSettings(true);
   }

   InitAssets();
   InitSigningContainer();
   InitAuthManager();
   InitChatView();

   statusBarView_ = std::make_shared<StatusBarView>(armory_, walletsMgr_, assetManager_, celerConnection_
      , signContainer_, ui_->statusbar);

   splashScreen.SetProgress(100);
   splashScreen.close();
   QApplication::processEvents();

   setupToolbar();
   setupMenu();

   ui_->widgetTransactions->setEnabled(false);

   connectSigner();
   connectArmory();

   InitChartsView();
   aboutDlg_ = std::make_shared<AboutDialog>(applicationSettings_->get<QString>(ApplicationSettings::ChangeLog_Base_Url), this);
   auto aboutDlgCb = [this] (int tab) {
      return [this, tab]() {
         aboutDlg_->setTab(tab);
         aboutDlg_->show();
      };
   };
   connect(ui_->actionAboutBlockSettle, &QAction::triggered, aboutDlgCb(0));
   connect(ui_->actionAboutTerminal, &QAction::triggered, aboutDlgCb(1));
   connect(ui_->actionContactBlockSettle, &QAction::triggered, aboutDlgCb(2));
   connect(ui_->actionVersion, &QAction::triggered, aboutDlgCb(3));

   ui_->tabWidget->setCurrentIndex(settings->get<int>(ApplicationSettings::GUI_main_tab));

   ui_->widgetTransactions->setAppSettings(applicationSettings_);

   UpdateMainWindowAppearence();
   setWidgetsAuthorized(false);

   updateControlEnabledState();

   InitWidgets();
}

void BSTerminalMainWindow::onMDConnectionDetailsRequired()
{
   networkSettingsLoader_ = std::make_unique<NetworkSettingsLoader>(logMgr_->logger()
      , applicationSettings_->pubBridgeHost(), applicationSettings_->pubBridgePort(), cbApprovePuB_);

   connect(networkSettingsLoader_.get(), &NetworkSettingsLoader::succeed, this, [this] {
      networkSettingsReceived(networkSettingsLoader_->settings());
      networkSettingsLoader_.reset();
   });

   connect(networkSettingsLoader_.get(), &NetworkSettingsLoader::failed, this, [this](const QString &errorMsg) {
      showError(tr("Network settings"), errorMsg);
      networkSettingsLoader_.reset();
   });

   networkSettingsLoader_->loadSettings();
}

void BSTerminalMainWindow::onBsConnectionFailed()
{
   SPDLOG_LOGGER_ERROR(logMgr_->logger(), "BsClient disconnected unexpectedly");
   onCelerDisconnected();
   showError(tr("Network error"), tr("Connection to BlockSettle server failed"));
}

void BSTerminalMainWindow::LoadCCDefinitionsFromPuB()
{
   if (!ccFileManager_) {
      return;
   }
   const auto &priWallet = walletsMgr_->getPrimaryWallet();
   if (priWallet) {
      const auto &ccGroup = priWallet->getGroup(bs::hd::BlockSettle_CC);
      if (ccGroup && (ccGroup->getNumLeaves() > 0)) {
         ccFileManager_->LoadCCDefinitionsFromPub();
      }
   }
}

void BSTerminalMainWindow::setWidgetsAuthorized(bool authorized)
{
   // Update authorized state for some widgets
   ui_->widgetPortfolio->setAuthorized(authorized);
   ui_->widgetRFQ->setAuthorized(authorized);
   ui_->widgetChart->setAuthorized(authorized);
}

void BSTerminalMainWindow::postSplashscreenActions()
{
   if (applicationSettings_->get<bool>(ApplicationSettings::SubscribeToMDOnStart)) {
      mdProvider_->SubscribeToMD();
   }
}

bool BSTerminalMainWindow::event(QEvent *event)
{
   if (event->type() == QEvent::WindowActivate) {
      auto tabChangedSignal = QMetaMethod::fromSignal(&QTabWidget::currentChanged);
      int currentIndex = ui_->tabWidget->currentIndex();
      tabChangedSignal.invoke(ui_->tabWidget, Q_ARG(int, currentIndex));
   }
   return QMainWindow::event(event);
}

BSTerminalMainWindow::~BSTerminalMainWindow()
{
   applicationSettings_->set(ApplicationSettings::GUI_main_geometry, geometry());
   applicationSettings_->set(ApplicationSettings::GUI_main_tab, ui_->tabWidget->currentIndex());
   applicationSettings_->SaveSettings();

   NotificationCenter::destroyInstance();
   if (signContainer_) {
      signContainer_->Stop();
      signContainer_.reset();
   }
   walletsMgr_.reset();
   assetManager_.reset();
   bs::UtxoReservation::destroy();
}

void BSTerminalMainWindow::setupToolbar()
{
   action_send_ = new QAction(tr("Create &Transaction"), this);
   connect(action_send_, &QAction::triggered, this, &BSTerminalMainWindow::onSend);
   action_receive_ = new QAction(tr("Generate &Address"), this);
   connect(action_receive_, &QAction::triggered, this, &BSTerminalMainWindow::onReceive);

   action_login_ = new QAction(tr("Login to BlockSettle"), this);
   connect(action_login_, &QAction::triggered, this, &BSTerminalMainWindow::onLogin);

   action_logout_ = new QAction(tr("Logout from BlockSettle"), this);
   connect(action_logout_, &QAction::triggered, this, &BSTerminalMainWindow::onLogout);

   auto toolBar = new QToolBar(this);
   toolBar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
   ui_->tabWidget->setCornerWidget(toolBar, Qt::TopRightCorner);

   // send bitcoins
   toolBar->addAction(action_send_);
   // receive bitcoins
   toolBar->addAction(action_receive_);

   action_logout_->setVisible(false);

   connect(ui_->pushButtonUser, &QPushButton::clicked, this, &BSTerminalMainWindow::onButtonUserClicked);

   QMenu* trayMenu = new QMenu(this);
   QAction* trayShowAction = trayMenu->addAction(tr("&Open Terminal"));
   connect(trayShowAction, &QAction::triggered, this, &QMainWindow::show);
   trayMenu->addSeparator();

   trayMenu->addAction(action_send_);
   trayMenu->addAction(action_receive_);
   trayMenu->addAction(ui_->actionSettings);

   trayMenu->addSeparator();
   trayMenu->addAction(ui_->actionQuit);
   sysTrayIcon_->setContextMenu(trayMenu);
}

void BSTerminalMainWindow::setupIcon()
{
   QIcon icon;
   QString iconFormatString = QString::fromStdString(":/ICON_BS_%1");

   for (const int s : {16, 24, 32}) {
      icon.addFile(iconFormatString.arg(s), QSize(s, s));
   }

   setWindowIcon(icon);

   sysTrayIcon_ = std::make_shared<QSystemTrayIcon>(icon, this);
   sysTrayIcon_->setToolTip(windowTitle());
   sysTrayIcon_->show();

   connect(sysTrayIcon_.get(), &QSystemTrayIcon::activated, [this](QSystemTrayIcon::ActivationReason reason) {
      if (reason == QSystemTrayIcon::Context) {
         // Right click, this is handled by the menu, so we don't do anything here.
         return;
      }

      setWindowState(windowState() & ~Qt::WindowMinimized);
      show();
      raise();
      activateWindow();
   });

   connect(qApp, &QCoreApplication::aboutToQuit, sysTrayIcon_.get(), &QSystemTrayIcon::hide);
   connect(qApp, SIGNAL(lastWindowClosed()), sysTrayIcon_.get(), SLOT(hide()));
}

void BSTerminalMainWindow::initConnections()
{
   connectionManager_ = std::make_shared<ConnectionManager>(logMgr_->logger("message"));

   celerConnection_ = std::make_shared<CelerClientProxy>(logMgr_->logger());
   connect(celerConnection_.get(), &BaseCelerClient::OnConnectedToServer, this, &BSTerminalMainWindow::onCelerConnected);
   connect(celerConnection_.get(), &BaseCelerClient::OnConnectionClosed, this, &BSTerminalMainWindow::onCelerDisconnected);
   connect(celerConnection_.get(), &BaseCelerClient::OnConnectionError, this, &BSTerminalMainWindow::onCelerConnectionError, Qt::QueuedConnection);

   mdProvider_ = std::make_shared<BSMarketDataProvider>(connectionManager_, logMgr_->logger("message"));
   connect(mdProvider_.get(), &MarketDataProvider::UserWantToConnectToMD, this, &BSTerminalMainWindow::acceptMDAgreement);
   connect(mdProvider_.get(), &MarketDataProvider::WaitingForConnectionDetails, this, &BSTerminalMainWindow::onMDConnectionDetailsRequired);
}

void BSTerminalMainWindow::LoadWallets()
{
   logMgr_->logger()->debug("Loading wallets");

   wasWalletsRegistered_ = false;
   walletsSynched_ = false;

   connect(walletsMgr_.get(), &bs::sync::WalletsManager::walletsReady, [this] {
      ui_->widgetRFQ->setWalletsManager(walletsMgr_);
      ui_->widgetRFQReply->setWalletsManager(walletsMgr_);
      autoSignQuoteProvider_->setWalletsManager(walletsMgr_);
   });
   connect(walletsMgr_.get(), &bs::sync::WalletsManager::walletsSynchronized, [this] {
      walletsSynched_ = true;
      QMetaObject::invokeMethod(this, [this] {
         updateControlEnabledState();
         CompleteDBConnection();
         act_->onRefresh({}, true);
      });
   });
   connect(walletsMgr_.get(), &bs::sync::WalletsManager::info, this, &BSTerminalMainWindow::showInfo);
   connect(walletsMgr_.get(), &bs::sync::WalletsManager::error, this, &BSTerminalMainWindow::showError);

   // Enable/disable send action when first wallet created/last wallet removed
   connect(walletsMgr_.get(), &bs::sync::WalletsManager::walletChanged, this
      , &BSTerminalMainWindow::updateControlEnabledState);
   connect(walletsMgr_.get(), &bs::sync::WalletsManager::newWalletAdded, this
      , &BSTerminalMainWindow::updateControlEnabledState);

   const auto &progressDelegate = [this](int cur, int total) {
//      const int progress = cur * (100 / total);
//      splashScreen.SetProgress(progress);
      logMgr_->logger()->debug("Loaded wallet {} of {}", cur, total);
   };
   walletsMgr_->reset();
   walletsMgr_->syncWallets(progressDelegate);
}

void BSTerminalMainWindow::InitAuthManager()
{
   authManager_ = std::make_shared<AuthAddressManager>(logMgr_->logger(), armory_, cbApprovePuB_);
   authManager_->init(applicationSettings_, walletsMgr_, signContainer_);

   connect(authManager_.get(), &AuthAddressManager::AddrStateChanged, [](const QString &addr, const QString &state) {
      NotificationCenter::notify(bs::ui::NotifyType::AuthAddress, { addr, state });
   });
   connect(authManager_.get(), &AuthAddressManager::AuthWalletCreated, [this](const QString &walletId) {
      if (authAddrDlg_ && walletId.isEmpty()) {
         openAuthManagerDialog();
      }
   });
}

std::shared_ptr<WalletSignerContainer> BSTerminalMainWindow::createSigner()
{
   if (signersProvider_->currentSignerIsLocal()) {
      return createLocalSigner();
   } else {
      return createRemoteSigner();
   }
}

std::shared_ptr<WalletSignerContainer> BSTerminalMainWindow::createRemoteSigner()
{
   SignerHost signerHost = signersProvider_->getCurrentSigner();
   QString resultPort = QString::number(signerHost.port);
   NetworkType netType = applicationSettings_->get<NetworkType>(ApplicationSettings::netType);

   // Define the callback that will be used to determine if the signer's BIP
   // 150 identity key, if it has changed, will be accepted. It needs strings
   // for the old and new keys, and a promise to set once the user decides.
   const auto &ourNewKeyCB = [this](const std::string& oldKey, const std::string& newKey
      , const std::string& srvAddrPort
      , const std::shared_ptr<FutureValue<bool>> &newKeyProm) {
      logMgr_->logger()->debug("[BSTerminalMainWindow::createSigner::callback] received"
         " new key {} [{}], old key {} [{}] for {} ({})", newKey, newKey.size(), oldKey
         , oldKey.size(), srvAddrPort, signersProvider_->getCurrentSigner().serverId());
      std::string oldKeyHex = oldKey;
      if (oldKeyHex.empty() && (signersProvider_->getCurrentSigner().serverId() == srvAddrPort)) {
         oldKeyHex = signersProvider_->getCurrentSigner().key.toStdString();
      }

      const auto &deferredDialog = [this, oldKeyHex, newKey, newKeyProm, srvAddrPort]{
         ImportKeyBox box(BSMessageBox::question
            , tr("Import Signer ID Key?")
            , this);

         box.setNewKey(newKey);
         box.setOldKey(QString::fromStdString(oldKeyHex));
         box.setAddrPort(srvAddrPort);

         const bool answer = (box.exec() == QDialog::Accepted);

         if (answer) {
            signersProvider_->addKey(srvAddrPort, newKey);
         }

         bool result = newKeyProm->setValue(answer);
         if (!result) {
            SPDLOG_LOGGER_DEBUG(logMgr_->logger()
               , "can't set result for signer key prompt for {}, perhaps connection was already closed"
               , srvAddrPort);
         }
      };

      addDeferredDialog(deferredDialog);
   };

   QString resultHost = signerHost.address;
   const auto remoteSigner = std::make_shared<RemoteSigner>(logMgr_->logger()
      , resultHost, resultPort, netType, connectionManager_
      , SignContainer::OpMode::Remote, false
      , signersProvider_->remoteSignerKeysDir(), signersProvider_->remoteSignerKeysFile(), ourNewKeyCB);

   ZmqBIP15XPeers peers;
   for (const auto &signer : signersProvider_->signers()) {
      try {
         const BinaryData signerKey = BinaryData::CreateFromHex(signer.key.toStdString());
         peers.push_back(ZmqBIP15XPeer(signer.serverId(), signerKey));
      }
      catch (const std::exception &e) {
         logMgr_->logger()->warn("[{}] invalid signer key: {}", __func__, e.what());
      }
   }
   remoteSigner->updatePeerKeys(peers);

   return remoteSigner;
}

std::shared_ptr<WalletSignerContainer> BSTerminalMainWindow::createLocalSigner()
{
   QLatin1String localSignerHost("127.0.0.1");
   QString localSignerPort = applicationSettings_->get<QString>(ApplicationSettings::localSignerPort);
   NetworkType netType = applicationSettings_->get<NetworkType>(ApplicationSettings::netType);

   if (SignerConnectionExists(localSignerHost, localSignerPort)) {
      BSMessageBox mbox(BSMessageBox::Type::question
                  , tr("Local Signer Connection")
                  , tr("Continue with Remote connection in Local GUI mode?")
                  , tr("The Terminal failed to spawn the headless signer as the program is already running. "
                       "Would you like to continue with remote connection in Local GUI mode?")
                  , this);
      if (mbox.exec() == QDialog::Rejected) {
         return nullptr;
      }

      // Use locally started signer as remote
      signersProvider_->switchToLocalFullGUI(localSignerHost, localSignerPort);
      return createRemoteSigner();
   }

   const bool startLocalSignerProcess = true;
   return std::make_shared<LocalSigner>(logMgr_->logger()
      , applicationSettings_->GetHomeDir(), netType
      , localSignerPort, connectionManager_
      , startLocalSignerProcess, "", ""
      , applicationSettings_->get<double>(ApplicationSettings::autoSignSpendLimit));
}

bool BSTerminalMainWindow::InitSigningContainer()
{
   // create local var just to avoid up-casting
   auto walletSignerContainer = createSigner();
   signContainer_ = walletSignerContainer;

   if (!signContainer_) {
      showError(tr("BlockSettle Signer"), tr("BlockSettle Signer creation failure"));
      return false;
   }
   connect(signContainer_.get(), &SignContainer::ready, this, &BSTerminalMainWindow::SignerReady, Qt::QueuedConnection);
   connect(signContainer_.get(), &SignContainer::connectionError, this, &BSTerminalMainWindow::onSignerConnError, Qt::QueuedConnection);
   connect(signContainer_.get(), &SignContainer::disconnected, this, &BSTerminalMainWindow::updateControlEnabledState, Qt::QueuedConnection);

   walletsMgr_->setSignContainer(walletSignerContainer);

   return true;
}

void BSTerminalMainWindow::SignerReady()
{
   updateControlEnabledState();

   LoadWallets();

   walletsMgr_->setUserId(BinaryData::CreateFromHex(celerConnection_->userId()));

   if (deferCCsync_) {
      signContainer_->syncCCNames(walletsMgr_->ccResolver()->securities());
      deferCCsync_ = false;
   }

   lastSignerError_ = SignContainer::NoError;
}

void BSTerminalMainWindow::acceptMDAgreement()
{
   const auto &deferredDailog = [this]{
      if (!isMDLicenseAccepted()) {
         MDAgreementDialog dlg{this};
         if (dlg.exec() != QDialog::Accepted) {
            return;
         }

         saveUserAcceptedMDLicense();
      }

      mdProvider_->MDLicenseAccepted();
   };

   addDeferredDialog(deferredDailog);
}

void BSTerminalMainWindow::updateControlEnabledState()
{
   if (action_send_) {
      action_send_->setEnabled(walletsMgr_->hdWalletsCount() > 0
         && armory_->isOnline() && signContainer_ && signContainer_->isReady());
   }
}

bool BSTerminalMainWindow::isMDLicenseAccepted() const
{
   return applicationSettings_->get<bool>(ApplicationSettings::MDLicenseAccepted);
}

void BSTerminalMainWindow::saveUserAcceptedMDLicense()
{
   applicationSettings_->set(ApplicationSettings::MDLicenseAccepted, true);
}

bool BSTerminalMainWindow::showStartupDialog()
{
   bool wasInitialized = applicationSettings_->get<bool>(ApplicationSettings::initialized);
   if (wasInitialized) {
     return true;
   }

 #ifdef _WIN32
   // Read registry value in case it was set with installer. Could be used only on Windows for now.
   QSettings settings(QLatin1String("HKEY_CURRENT_USER\\Software\\blocksettle\\blocksettle"), QSettings::NativeFormat);
   bool showLicense = !settings.value(QLatin1String("license_accepted"), false).toBool();
 #else
   bool showLicense = true;
 #endif // _WIN32

   StartupDialog startupDialog(showLicense);
   startupDialog.init(applicationSettings_, armoryServersProvider_);
   int result = startupDialog.exec();

   if (result == QDialog::Rejected) {
      hide();
      return false;
   }

   // Ueed update armory settings if case user selects TestNet
   // (MainNet selected by default at startup)
   applicationSettings_->selectNetwork();

   return true;
}

void BSTerminalMainWindow::InitAssets()
{
   ccFileManager_ = std::make_shared<CCFileManager>(logMgr_->logger(), applicationSettings_
      , connectionManager_, cbApprovePuB_);
   assetManager_ = std::make_shared<AssetManager>(logMgr_->logger(), walletsMgr_, mdProvider_, celerConnection_);
   assetManager_->init();

   orderListModel_ = std::make_unique<OrderListModel>(assetManager_);

   connect(ccFileManager_.get(), &CCFileManager::CCSecurityDef, assetManager_.get(), &AssetManager::onCCSecurityReceived);
   connect(ccFileManager_.get(), &CCFileManager::CCSecurityInfo, walletsMgr_.get(), &bs::sync::WalletsManager::onCCSecurityInfo);
   connect(ccFileManager_.get(), &CCFileManager::Loaded, this, &BSTerminalMainWindow::onCCLoaded);
   connect(ccFileManager_.get(), &CCFileManager::LoadingFailed, this, &BSTerminalMainWindow::onCCInfoMissing);

   connect(mdProvider_.get(), &MarketDataProvider::MDUpdate, assetManager_.get(), &AssetManager::onMDUpdate);

   if (ccFileManager_->hasLocalFile()) {
      ccFileManager_->LoadSavedCCDefinitions();
   }
}

void BSTerminalMainWindow::InitPortfolioView()
{
   portfolioModel_ = std::make_shared<CCPortfolioModel>(walletsMgr_, assetManager_, this);
   ui_->widgetPortfolio->init(applicationSettings_, mdProvider_, portfolioModel_,
                             signContainer_, armory_, logMgr_->logger("ui"),
                             walletsMgr_);
}

void BSTerminalMainWindow::InitWalletsView()
{
   ui_->widgetWallets->init(logMgr_->logger("ui"), walletsMgr_, signContainer_
      , applicationSettings_, connectionManager_, assetManager_, authManager_, armory_);
}

void BSTerminalMainWindow::InitChatView()
{
   chatClientServicePtr_ = std::make_shared<Chat::ChatClientService>();

   connect(chatClientServicePtr_.get(), &Chat::ChatClientService::initDone, [this]() {
      ui_->widgetChat->init(connectionManager_, applicationSettings_, chatClientServicePtr_,
         logMgr_->logger("chat"), walletsMgr_, authManager_, armory_, signContainer_, mdProvider_, assetManager_);
   });

   chatClientServicePtr_->Init(connectionManager_, applicationSettings_, logMgr_->logger("chat"));

   connect(ui_->tabWidget, &QTabWidget::currentChanged, this, &BSTerminalMainWindow::onTabWidgetCurrentChanged);
   connect(ui_->widgetChat, &ChatWidget::requestPrimaryWalletCreation, this, &BSTerminalMainWindow::onCreatePrimaryWalletRequest);

   if (NotificationCenter::instance() != nullptr) {
      connect(NotificationCenter::instance(), &NotificationCenter::newChatMessageClick, ui_->widgetChat, &ChatWidget::onNewChatMessageTrayNotificationClicked);
   }
}

void BSTerminalMainWindow::InitChartsView()
{
    ui_->widgetChart->init(applicationSettings_, mdProvider_, connectionManager_, logMgr_->logger("ui"));
}

// Initialize widgets related to transactions.
void BSTerminalMainWindow::InitTransactionsView()
{
   ui_->widgetExplorer->init(armory_, logMgr_->logger(), walletsMgr_, ccFileManager_, authManager_);
   ui_->widgetTransactions->init(walletsMgr_, armory_, signContainer_,
                                logMgr_->logger("ui"));
   ui_->widgetTransactions->setEnabled(true);

   ui_->widgetTransactions->SetTransactionsModel(transactionsModel_);
   ui_->widgetPortfolio->SetTransactionsModel(transactionsModel_);
}

void BSTerminalMainWindow::MainWinACT::onStateChanged(ArmoryState state)
{
   switch (state) {
   case ArmoryState::Ready:
      QMetaObject::invokeMethod(parent_, [this] {
         parent_->isArmoryReady_ = true;
         parent_->CompleteDBConnection();
         parent_->CompleteUIOnlineView();
         parent_->walletsMgr_->goOnline();
      });
      break;
   case ArmoryState::Connected:
      QMetaObject::invokeMethod(parent_, [this] {
         parent_->wasWalletsRegistered_ = false;
         parent_->armory_->goOnline();
      });
      break;
   case ArmoryState::Offline:
      QMetaObject::invokeMethod(parent_, &BSTerminalMainWindow::ArmoryIsOffline);
      break;
   default:
      break;
   }
}

void BSTerminalMainWindow::MainWinACT::onRefresh(const std::vector<BinaryData> &, bool)
{
   if (!parent_->initialWalletCreateDialogShown_ && parent_->walletsMgr_
      && parent_->walletsMgr_->isWalletsReady()
      && (parent_->walletsMgr_->hdWalletsCount() == 0)) {

      const auto &deferredDialog = [this]{
         parent_->createWallet(true, [] {});
      };

      parent_->addDeferredDialog(deferredDialog);
   }
   parent_->initialWalletCreateDialogShown_ = true;
}

void BSTerminalMainWindow::CompleteUIOnlineView()
{
   if (!transactionsModel_) {
      transactionsModel_ = std::make_shared<TransactionsViewModel>(armory_
         , walletsMgr_, logMgr_->logger("ui"), this);

      InitTransactionsView();
      transactionsModel_->loadAllWallets();
   }
   updateControlEnabledState();
}

void BSTerminalMainWindow::CompleteDBConnection()
{
   if (!wasWalletsRegistered_ && walletsSynched_ && isArmoryReady_) {
      // Fix race with BDMAction_Refresh and BDMAction_Ready: register wallets AFTER armory becames ready.
      // Otherwise BDMAction_Refresh might come before BDMAction_Ready causing a lot of problems.
      walletsMgr_->registerWallets();
      wasWalletsRegistered_ = true;
   }
}

void BSTerminalMainWindow::onReactivate()
{
   show();
}

void BSTerminalMainWindow::raiseWindow()
{
   if (isMinimized()) {
      showNormal();
   } else if (isHidden()) {
      show();
   }
   raise();
   activateWindow();
   setFocus();
#ifdef Q_OS_WIN
   auto hwnd = reinterpret_cast<HWND>(winId());
   auto flags = static_cast<UINT>(SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
   auto currentProcessId = ::GetCurrentProcessId();
   auto currentThreadId = ::GetCurrentThreadId();
   auto windowThreadId = ::GetWindowThreadProcessId(hwnd, nullptr);
   if (currentThreadId != windowThreadId) {
      ::AttachThreadInput(windowThreadId, currentThreadId, TRUE);
   }
   ::AllowSetForegroundWindow(currentProcessId);
   ::SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, flags);
   ::SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, flags);
   ::SetForegroundWindow(hwnd);
   ::SetFocus(hwnd);
   ::SetActiveWindow(hwnd);
   if (currentThreadId != windowThreadId) {
      ::AttachThreadInput(windowThreadId, currentThreadId, FALSE);
   }
#endif // Q_OS_WIN
}

void BSTerminalMainWindow::UpdateMainWindowAppearence()
{
   if (!applicationSettings_->get<bool>(ApplicationSettings::closeToTray) && isHidden()) {
      setWindowState(windowState() & ~Qt::WindowMinimized);
      show();
      raise();
      activateWindow();
   }

   setWindowTitle(tr("BlockSettle Terminal"));

//   const auto bsTitle = tr("BlockSettle Terminal [%1]");
//   switch (applicationSettings_->get<NetworkType>(ApplicationSettings::netType)) {
//   case NetworkType::TestNet:
//      setWindowTitle(bsTitle.arg(tr("TESTNET")));
//      break;

//   case NetworkType::RegTest:
//      setWindowTitle(bsTitle.arg(tr("REGTEST")));
//      break;

//   default:
//      setWindowTitle(tr("BlockSettle Terminal"));
//      break;
//   }
}

bool BSTerminalMainWindow::isUserLoggedIn() const
{
   return (celerConnection_ && celerConnection_->IsConnected());
}

bool BSTerminalMainWindow::isArmoryConnected() const
{
   return armory_->state() == ArmoryState::Ready;
}

void BSTerminalMainWindow::ArmoryIsOffline()
{
   logMgr_->logger("ui")->debug("BSTerminalMainWindow::ArmoryIsOffline");
   if (walletsMgr_) {
      walletsMgr_->unregisterWallets();
   }
   connectArmory();
   updateControlEnabledState();
   // XXX: disabled until armory connection is stable in terminal
   // updateLoginActionState();
}

void BSTerminalMainWindow::initArmory()
{
   armory_ = std::make_shared<ArmoryObject>(logMgr_->logger()
      , applicationSettings_->get<std::string>(ApplicationSettings::txCacheFileName), true);
   act_ = make_unique<MainWinACT>(this);
   act_->init(armory_.get());
}

void BSTerminalMainWindow::MainWinACT::onTxBroadcastError(const std::string &hash, const std::string &err)
{
   NotificationCenter::notify(bs::ui::NotifyType::BroadcastError, { QString::fromStdString(hash)
      , QString::fromStdString(err) });
}

void BSTerminalMainWindow::MainWinACT::onZCReceived(const std::vector<bs::TXEntry> &zcs)
{
   QMetaObject::invokeMethod(parent_, [this, zcs] { parent_->onZCreceived(zcs); });
}

void BSTerminalMainWindow::connectArmory()
{
   ArmorySettings currentArmorySettings = armoryServersProvider_->getArmorySettings();
   armoryServersProvider_->setConnectedArmorySettings(currentArmorySettings);
   armory_->setupConnection(currentArmorySettings, [this](const BinaryData& srvPubKey, const std::string& srvIPPort) {
      auto promiseObj = std::make_shared<std::promise<bool>>();
      std::future<bool> futureObj = promiseObj->get_future();
      QMetaObject::invokeMethod(this, [this, srvPubKey, srvIPPort, promiseObj] {
         showArmoryServerPrompt(srvPubKey, srvIPPort, promiseObj);
      });

      bool result = futureObj.get();
      // stop armory connection loop if server key was rejected
      if (!result) {
         armory_->needsBreakConnectionLoop_.store(true);
         armory_->setState(ArmoryState::Cancelled);
      }
      return result;
   });
}

void BSTerminalMainWindow::connectSigner()
{
   if (!signContainer_) {
      return;
   }

   if (!signContainer_->Start()) {
      BSMessageBox(BSMessageBox::warning, tr("BlockSettle Signer Connection")
         , tr("Failed to start signer connection.")).exec();
   }
}

bool BSTerminalMainWindow::createWallet(bool primary, const std::function<void()> &cb
   , bool reportSuccess)
{
   if (primary && (walletsMgr_->hdWalletsCount() > 0)) {
      auto wallet = walletsMgr_->getHDWallet(0);
      if (wallet->isPrimary()) {
         if (cb) {
            cb();
         }
         return true;
      }
      BSMessageBox qry(BSMessageBox::question, tr("Promote to primary wallet"), tr("Promote to primary wallet?")
         , tr("To trade through BlockSettle, you are required to have a wallet which"
            " supports the sub-wallets required to interact with the system. Each Terminal"
            " may only have one Primary Wallet. Do you wish to promote '%1'?")
         .arg(QString::fromStdString(wallet->name())), this);
      if (qry.exec() == QDialog::Accepted) {
         walletsMgr_->PromoteHDWallet(wallet->walletId(), [cb](bs::error::ErrorCode result) {
            if ((result == bs::error::ErrorCode::NoError) && cb) {
               cb();
            }
         });
         return true;
      }

      return false;
   }

   if (!signContainer_->isOffline()) {
      NewWalletDialog newWalletDialog(true, applicationSettings_, this);
      if (newWalletDialog.exec() != QDialog::Accepted) {
         return false;
      }

      if (newWalletDialog.isCreate()) {
         if (ui_->widgetWallets->CreateNewWallet(reportSuccess) && cb) {
            cb();
         }
      }
      else if (newWalletDialog.isImport()) {
         if (ui_->widgetWallets->ImportNewWallet(reportSuccess) && cb) {
            cb();
         }
      }
   } else {
      if (ui_->widgetWallets->ImportNewWallet(reportSuccess) && cb) {
         cb();
      }
   }

   return true;
}

void BSTerminalMainWindow::onCreatePrimaryWalletRequest()
{
   bool result = createWallet(true, [] {});

   if (!result) {
      // Need to inform UI about rejection
      ui_->widgetRFQ->forceCheckCondition();
      ui_->widgetRFQReply->forceCheckCondition();
      ui_->widgetChat->onUpdateOTCShield();
   }
}

void BSTerminalMainWindow::showInfo(const QString &title, const QString &text)
{
   BSMessageBox(BSMessageBox::info, title, text).exec();
}

void BSTerminalMainWindow::showError(const QString &title, const QString &text)
{
   QMetaObject::invokeMethod(this, [this, title, text] {
      BSMessageBox(BSMessageBox::critical, title, text, this).exec();
   });
}

void BSTerminalMainWindow::onSignerConnError(SignContainer::ConnectionError error, const QString &details)
{
   updateControlEnabledState();

   // Prevent showing multiple signer error dialogs (for example network mismatch)
   if (error == lastSignerError_) {
      return;
   }
   lastSignerError_ = error;

   if (error != SignContainer::ConnectionTimeout || signContainer_->isLocal()) {
      showError(tr("Signer Connection Error"), details);
   }
}

void BSTerminalMainWindow::onReceive()
{
   const auto defWallet = walletsMgr_->getDefaultWallet();
   std::string selWalletId = defWallet ? defWallet->walletId() : std::string{};
   if (ui_->tabWidget->currentWidget() == ui_->widgetWallets) {
      auto wallets = ui_->widgetWallets->getSelectedWallets();
      if (!wallets.empty()) {
         selWalletId = wallets[0]->walletId();
      } else {
         wallets = ui_->widgetWallets->getFirstWallets();

         if (!wallets.empty()) {
            selWalletId = wallets[0]->walletId();
         }
      }
   }
   SelectWalletDialog *selectWalletDialog = new SelectWalletDialog(
      walletsMgr_, selWalletId, this);
   selectWalletDialog->exec();

   if (selectWalletDialog->result() == QDialog::Rejected) {
      return;
   }

   NewAddressDialog* newAddressDialog = new NewAddressDialog(
      selectWalletDialog->getSelectedWallet(), signContainer_, this);
   newAddressDialog->show();
}

void BSTerminalMainWindow::createAdvancedTxDialog(const std::string &selectedWalletId)
{
   CreateTransactionDialogAdvanced advancedDialog{armory_, walletsMgr_
      , signContainer_, true, logMgr_->logger("ui"), applicationSettings_, nullptr, this};

   if (!selectedWalletId.empty()) {
      advancedDialog.SelectWallet(selectedWalletId);
   }

   advancedDialog.exec();
}

void BSTerminalMainWindow::onSend()
{
   std::string selectedWalletId;

   if (ui_->tabWidget->currentWidget() == ui_->widgetWallets) {
      auto wallet = ui_->widgetWallets->getSelectedHdWallet();
      if (!wallet) {
         wallet = walletsMgr_->getPrimaryWallet();
      }
      if (wallet) {
         selectedWalletId = wallet->walletId();
      }
   }

   if (QGuiApplication::keyboardModifiers() & Qt::ShiftModifier) {
      createAdvancedTxDialog(selectedWalletId);
   } else {
      if (applicationSettings_->get<bool>(ApplicationSettings::AdvancedTxDialogByDefault)) {
         createAdvancedTxDialog(selectedWalletId);
      } else {
         CreateTransactionDialogSimple dlg(armory_, walletsMgr_, signContainer_
            , logMgr_->logger("ui"), applicationSettings_, this);

         if (!selectedWalletId.empty()) {
            dlg.SelectWallet(selectedWalletId);
         }

         dlg.exec();

         if ((dlg.result() == QDialog::Accepted) && dlg.userRequestedAdvancedDialog()) {
            auto advancedDialog = dlg.CreateAdvancedDialog();

            advancedDialog->exec();
         }
      }
   }
}

void BSTerminalMainWindow::setupMenu()
{
   // menu role erquired for OSX only, to place it to first menu item
   action_login_->setMenuRole(QAction::ApplicationSpecificRole);
   action_logout_->setMenuRole(QAction::ApplicationSpecificRole);

   ui_->menuFile->insertAction(ui_->actionSettings, action_login_);
   ui_->menuFile->insertAction(ui_->actionSettings, action_logout_);

   ui_->menuFile->insertSeparator(action_login_);
   ui_->menuFile->insertSeparator(ui_->actionSettings);

   connect(ui_->actionCreateNewWallet, &QAction::triggered, this, [ww = ui_->widgetWallets]{ ww->CreateNewWallet(); });
   connect(ui_->actionAuthenticationAddresses, &QAction::triggered, this, &BSTerminalMainWindow::openAuthManagerDialog);
   connect(ui_->actionSettings, &QAction::triggered, this, [=]() { openConfigDialog(); });
   connect(ui_->actionAccountInformation, &QAction::triggered, this, &BSTerminalMainWindow::openAccountInfoDialog);
   connect(ui_->actionEnterColorCoinToken, &QAction::triggered, this, &BSTerminalMainWindow::openCCTokenDialog);

   onUserLoggedOut();

#ifndef Q_OS_MAC
   ui_->horizontalFrame->hide();

   ui_->menubar->setCornerWidget(ui_->pushButtonUser);
#endif
}

void BSTerminalMainWindow::openAuthManagerDialog()
{
   openAuthDlgVerify(QString());
}

void BSTerminalMainWindow::openAuthDlgVerify(const QString &addrToVerify)
{
   const auto showAuthDlg = [this, addrToVerify] {
      authAddrDlg_->show();
      QApplication::processEvents();
      authAddrDlg_->setAddressToVerify(addrToVerify);
   };
   if (authManager_->HaveAuthWallet()) {
      showAuthDlg();
   } else {
      createAuthWallet([this, showAuthDlg] {
         QMetaObject::invokeMethod(this, showAuthDlg);
      });
   }
}

void BSTerminalMainWindow::openConfigDialog()
{
   ConfigDialog configDialog(applicationSettings_, armoryServersProvider_, signersProvider_, signContainer_, this);
   connect(&configDialog, &ConfigDialog::reconnectArmory, this, &BSTerminalMainWindow::onArmoryNeedsReconnect);
   configDialog.exec();

   UpdateMainWindowAppearence();
}

void BSTerminalMainWindow::openAccountInfoDialog()
{
   CelerAccountInfoDialog dialog(celerConnection_, this);
   dialog.exec();
}

void BSTerminalMainWindow::openCCTokenDialog()
{
   const auto lbdCCTokenDlg = [this] {
      QMetaObject::invokeMethod(this, [this] {
         CCTokenEntryDialog(walletsMgr_, ccFileManager_, this).exec();
      });
   };
   // Do not use deferredDialogs_ here as it will deadblock PuB public key processing
   if (walletsMgr_->hasPrimaryWallet()) {
      lbdCCTokenDlg();
   }
   else {
      createWallet(true, lbdCCTokenDlg, false);
   }
}

void BSTerminalMainWindow::onLogin()
{
   LoginWindow loginDialog(logMgr_->logger("autheID"), applicationSettings_, &cbApprovePuB_, &cbApproveProxy_, this);

   int rc = loginDialog.exec();

   if (rc != QDialog::Accepted && !loginDialog.result()) {
      setWidgetsAuthorized(false);
      return;
   }

   currentUserLogin_ = loginDialog.email();

   ui_->widgetChat->setUserType(loginDialog.result()->userType);
   chatClientServicePtr_->LoginToServer(loginDialog.result()->chatTokenData, loginDialog.result()->chatTokenSign, cbApproveChat_);

   bsClient_ = loginDialog.getClient();
   ccFileManager_->setBsClient(bsClient_.get());
   authAddrDlg_->setBsClient(bsClient_.get());

   connect(bsClient_.get(), &BsClient::connectionFailed, this, &BSTerminalMainWindow::onBsConnectionFailed);

   // connect to RFQ dialog
   connect(bsClient_.get(), &BsClient::processPbMessage, ui_->widgetRFQ, &RFQRequestWidget::onMessageFromPB);
   connect(ui_->widgetRFQ, &RFQRequestWidget::sendUnsignedPayinToPB, bsClient_.get(), &BsClient::sendUnsignedPayin);
   connect(ui_->widgetRFQ, &RFQRequestWidget::sendSignedPayinToPB, bsClient_.get(), &BsClient::sendSignedPayin);
   connect(ui_->widgetRFQ, &RFQRequestWidget::sendSignedPayoutToPB, bsClient_.get(), &BsClient::sendSignedPayout);

   // connect to quote dialog
   connect(bsClient_.get(), &BsClient::processPbMessage, ui_->widgetRFQReply, &RFQReplyWidget::onMessageFromPB);
   connect(ui_->widgetRFQReply, &RFQReplyWidget::sendUnsignedPayinToPB, bsClient_.get(), &BsClient::sendUnsignedPayin);
   connect(ui_->widgetRFQReply, &RFQReplyWidget::sendSignedPayinToPB, bsClient_.get(), &BsClient::sendSignedPayin);
   connect(ui_->widgetRFQReply, &RFQReplyWidget::sendSignedPayoutToPB, bsClient_.get(), &BsClient::sendSignedPayout);

   connect(bsClient_.get(), &BsClient::processPbMessage, orderListModel_.get(), &OrderListModel::onMessageFromPB);

   networkSettingsReceived(loginDialog.networkSettings());

   authManager_->ConnectToPublicBridge(connectionManager_, celerConnection_);

   setLoginButtonText(currentUserLogin_);
   setWidgetsAuthorized(true);

   // We don't use password here, BsProxy will manage authentication
   SPDLOG_LOGGER_DEBUG(logMgr_->logger(), "got celer login: {}", loginDialog.result()->celerLogin);
   celerConnection_->LoginToServer(bsClient_.get(), loginDialog.result()->celerLogin, loginDialog.email().toStdString());

   ui_->widgetWallets->setUsername(currentUserLogin_);
   action_logout_->setVisible(false);
   action_login_->setEnabled(false);

   // Market data, charts and chat should be available for all Auth eID logins
   mdProvider_->SubscribeToMD();

   LoadCCDefinitionsFromPuB();

   connect(bsClient_.get(), &BsClient::processPbMessage, ui_->widgetChat, &ChatWidget::onProcessOtcPbMessage);
   connect(ui_->widgetChat, &ChatWidget::sendOtcPbMessage, bsClient_.get(), &BsClient::sendPbMessage);
}

void BSTerminalMainWindow::onLogout()
{
   ui_->widgetWallets->setUsername(QString());
   chatClientServicePtr_->LogoutFromServer();
   ui_->widgetChart->disconnect();

   if (celerConnection_->IsConnected()) {
      celerConnection_->CloseConnection();
   }

   mdProvider_->UnsubscribeFromMD();

   setLoginButtonText(loginButtonText_);

   setWidgetsAuthorized(false);

   bsClient_.reset();
}

void BSTerminalMainWindow::onUserLoggedIn()
{
   ui_->actionAccountInformation->setEnabled(true);
   ui_->actionAuthenticationAddresses->setEnabled(celerConnection_->celerUserType()
      != BaseCelerClient::CelerUserType::Market);
   ui_->actionOneTimePassword->setEnabled(true);
   ui_->actionEnterColorCoinToken->setEnabled(true);

   ui_->actionDeposits->setEnabled(true);
   ui_->actionWithdrawalRequest->setEnabled(true);
   ui_->actionLinkAdditionalBankAccount->setEnabled(true);

   ccFileManager_->LoadCCDefinitionsFromPub();
   ccFileManager_->ConnectToCelerClient(celerConnection_);

   const auto userId = BinaryData::CreateFromHex(celerConnection_->userId());
   const auto &deferredDialog = [this, userId] {
      walletsMgr_->setUserId(userId);
   };
   addDeferredDialog(deferredDialog);

   setLoginButtonText(currentUserLogin_);
}

void BSTerminalMainWindow::onUserLoggedOut()
{
   ui_->actionAccountInformation->setEnabled(false);
   ui_->actionAuthenticationAddresses->setEnabled(false);
   ui_->actionEnterColorCoinToken->setEnabled(false);
   ui_->actionOneTimePassword->setEnabled(false);

   ui_->actionDeposits->setEnabled(false);
   ui_->actionWithdrawalRequest->setEnabled(false);
   ui_->actionLinkAdditionalBankAccount->setEnabled(false);

   if (walletsMgr_) {
      walletsMgr_->setUserId(BinaryData{});
   }
   if (authManager_) {
      authManager_->OnDisconnectedFromCeler();
   }
}

void BSTerminalMainWindow::onCelerConnected()
{
   action_login_->setVisible(false);
   action_logout_->setVisible(true);

   onUserLoggedIn();
}

void BSTerminalMainWindow::onCelerDisconnected()
{
   action_logout_->setVisible(false);
   action_login_->setEnabled(true);
   action_login_->setVisible(true);

   onUserLoggedOut();
   celerConnection_->CloseConnection();
}

void BSTerminalMainWindow::onCelerConnectionError(int errorCode)
{
   switch(errorCode)
   {
   case BaseCelerClient::LoginError:
      logMgr_->logger("ui")->debug("[BSTerminalMainWindow::onCelerConnectionError] login failed. Probably user do not have BS matching account");
      break;
   }
}

void BSTerminalMainWindow::createAuthWallet(const std::function<void()> &cb)
{
   if (celerConnection_->tradingAllowed()) {
      const auto &deferredDialog = [this, cb]{
         const auto lbdCreateAuthWallet = [this, cb] {
            QMetaObject::invokeMethod(this, [this, cb] {
               if (walletsMgr_->getAuthWallet()) {
                  if (cb) {
                     cb();
                  }
               }
               else {
                  BSMessageBox createAuthReq(BSMessageBox::question, tr("Authentication Wallet")
                     , tr("Create Authentication Wallet")
                     , tr("You don't have a sub-wallet in which to hold Authentication Addresses."
                        " Would you like to create one?"), this);
                  if (createAuthReq.exec() == QDialog::Accepted) {
                     authManager_->createAuthWallet(cb);
                  }
               }
            });
         };
         if (walletsMgr_->hasPrimaryWallet()) {
            lbdCreateAuthWallet();
         }
         else {
            createWallet(true, lbdCreateAuthWallet);
         }
      };
      addDeferredDialog(deferredDialog);
   }
}

struct BSTerminalMainWindow::TxInfo {
   Tx       tx;
   uint32_t txTime{};
   int64_t  value{};
   std::shared_ptr<bs::sync::Wallet>   wallet;
   bs::sync::Transaction::Direction    direction{};
   QString  mainAddress;
};

void BSTerminalMainWindow::onZCreceived(const std::vector<bs::TXEntry> &entries)
{
   if (entries.empty()) {
      return;
   }
   for (const auto &entry : entries) {
      const auto &cbTx = [this, id = entry.walletId, txTime = entry.txTime, value = entry.value](const Tx &tx) {
         const auto wallet = walletsMgr_->getWalletById(id);
         if (!wallet) {
            return;
         }

         auto txInfo = std::make_shared<TxInfo>();
         txInfo->tx = tx;
         txInfo->txTime = txTime;
         txInfo->value = value;
         txInfo->wallet = wallet;

         const auto &cbDir = [this, txInfo] (bs::sync::Transaction::Direction dir, const std::vector<bs::Address> &) {
            txInfo->direction = dir;
            if (!txInfo->mainAddress.isEmpty()) {
               showZcNotification(*txInfo);
            }
         };

         const auto &cbMainAddr = [this, txInfo] (const QString &mainAddr, int addrCount) {
            txInfo->mainAddress = mainAddr;
            if ((txInfo->direction != bs::sync::Transaction::Direction::Unknown)) {
               showZcNotification(*txInfo);
            }
         };

         walletsMgr_->getTransactionDirection(tx, id, cbDir);
         walletsMgr_->getTransactionMainAddress(tx, id, (value > 0), cbMainAddr);
      };
      armory_->getTxByHash(entry.txHash, cbTx);
   }
}

void BSTerminalMainWindow::showZcNotification(const TxInfo &txInfo)
{
   QStringList lines;
   lines << tr("Date: %1").arg(UiUtils::displayDateTime(txInfo.txTime));
   lines << tr("TX: %1 %2 %3").arg(tr(bs::sync::Transaction::toString(txInfo.direction)))
      .arg(txInfo.wallet->displayTxValue(txInfo.value)).arg(txInfo.wallet->displaySymbol());
   lines << tr("Wallet: %1").arg(QString::fromStdString(txInfo.wallet->name()));
   lines << txInfo.mainAddress;

   const auto &title = tr("New blockchain transaction");
   NotificationCenter::notify(bs::ui::NotifyType::BlockchainTX, { title, lines.join(tr("\n")) });
}

void BSTerminalMainWindow::showRunInBackgroundMessage()
{
   sysTrayIcon_->showMessage(tr("BlockSettle is running"), tr("BlockSettle Terminal is running in the backgroud. Click the tray icon to open the main window."), QIcon(QLatin1String(":/resources/login-logo.png")));
}

void BSTerminalMainWindow::closeEvent(QCloseEvent* event)
{
   if (applicationSettings_->get<bool>(ApplicationSettings::closeToTray)) {
      hide();
      event->ignore();
   }
   else {
      chatClientServicePtr_->LogoutFromServer();

      QMainWindow::closeEvent(event);
      QApplication::exit();
   }
}

void BSTerminalMainWindow::changeEvent(QEvent* e)
{
   switch (e->type())
   {
      case QEvent::WindowStateChange:
      {
         if (this->windowState() & Qt::WindowMinimized)
         {
            if (applicationSettings_->get<bool>(ApplicationSettings::minimizeToTray))
            {
               QTimer::singleShot(0, this, &QMainWindow::hide);
            }
         }

         break;
      }
      default:
         break;
   }

   QMainWindow::changeEvent(e);
}

void BSTerminalMainWindow::setLoginButtonText(const QString& text)
{
   ui_->pushButtonUser->setText(text);

#ifndef Q_OS_MAC
   ui_->menubar->adjustSize();
#endif
}

void BSTerminalMainWindow::onCCLoaded()
{
   walletsMgr_->onCCInfoLoaded();

   const auto ccResolver = walletsMgr_->ccResolver();
   if (ccResolver && signContainer_) {
      deferCCsync_ = false;
      signContainer_->syncCCNames(ccResolver->securities());
   }
   else {
      deferCCsync_ = true;
   }
}

void BSTerminalMainWindow::onCCInfoMissing()
{ }   // do nothing here since we don't know if user will need Private Market before logon to Celer

void BSTerminalMainWindow::setupShortcuts()
{
   auto overviewTabShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+1")), this);
   overviewTabShortcut->setContext(Qt::WindowShortcut);
   connect(overviewTabShortcut, &QShortcut::activated, [this](){ ui_->tabWidget->setCurrentIndex(0);});

   auto tradingTabShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+2")), this);
   tradingTabShortcut->setContext(Qt::WindowShortcut);
   connect(tradingTabShortcut, &QShortcut::activated, [this](){ ui_->tabWidget->setCurrentIndex(1);});

   auto dealingTabShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+3")), this);
   dealingTabShortcut->setContext(Qt::WindowShortcut);
   connect(dealingTabShortcut, &QShortcut::activated, [this](){ ui_->tabWidget->setCurrentIndex(2);});

   auto walletsTabShortcutt = new QShortcut(QKeySequence(QStringLiteral("Ctrl+4")), this);
   walletsTabShortcutt->setContext(Qt::WindowShortcut);
   connect(walletsTabShortcutt, &QShortcut::activated, [this](){ ui_->tabWidget->setCurrentIndex(3);});

   auto transactionsTabShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+5")), this);
   transactionsTabShortcut->setContext(Qt::WindowShortcut);
   connect(transactionsTabShortcut, &QShortcut::activated, [this](){ ui_->tabWidget->setCurrentIndex(4);});

   // TODO: Switch ChatWidget to TabWithShortcut if needed (it will ignore shortcuts right now)

   auto addShotcut = [this](const char *keySequence, TabWithShortcut::ShortcutType type) {
      auto shortcut = new QShortcut(QKeySequence(QLatin1String(keySequence)), this);
      shortcut->setContext(Qt::WindowShortcut);
      connect(shortcut, &QShortcut::activated, [this, type]() {
         auto widget = dynamic_cast<TabWithShortcut*>(ui_->tabWidget->currentWidget());
         if (widget) {
            widget->shortcutActivated(type);
         }
      });
   };

   addShotcut("Alt+1", TabWithShortcut::ShortcutType::Alt_1);
   addShotcut("Alt+2", TabWithShortcut::ShortcutType::Alt_2);
   addShotcut("Alt+3", TabWithShortcut::ShortcutType::Alt_3);
   addShotcut("Ctrl+S", TabWithShortcut::ShortcutType::Ctrl_S);
   addShotcut("Ctrl+P", TabWithShortcut::ShortcutType::Ctrl_P);
   addShotcut("Ctrl+Q", TabWithShortcut::ShortcutType::Ctrl_Q);
   addShotcut("Alt+S", TabWithShortcut::ShortcutType::Alt_S);
   addShotcut("Alt+B", TabWithShortcut::ShortcutType::Alt_B);
   addShotcut("Alt+P", TabWithShortcut::ShortcutType::Alt_P);
}

void BSTerminalMainWindow::onButtonUserClicked() {
   if (ui_->pushButtonUser->text() == loginButtonText_) {
      onLogin();
   } else {
      if (BSMessageBox(BSMessageBox::question, tr("User Logout"), tr("You are about to logout")
         , tr("Do you want to continue?")).exec() == QDialog::Accepted)
      onLogout();
   }
}

void BSTerminalMainWindow::showArmoryServerPrompt(const BinaryData &srvPubKey, const std::string &srvIPPort, std::shared_ptr<std::promise<bool>> promiseObj)
{
   QList<ArmoryServer> servers = armoryServersProvider_->servers();
   int serverIndex = armoryServersProvider_->indexOfIpPort(srvIPPort);
   if (serverIndex >= 0) {
      ArmoryServer server = servers.at(serverIndex);

      const auto &deferredDialog = [this, server, promiseObj, srvPubKey, srvIPPort]{
         if (server.armoryDBKey.isEmpty()) {
            ImportKeyBox box(BSMessageBox::question
               , tr("Import ArmoryDB ID Key?")
               , this);

            box.setNewKeyFromBinary(srvPubKey);
            box.setAddrPort(srvIPPort);

            bool answer = (box.exec() == QDialog::Accepted);

            if (answer) {
               armoryServersProvider_->addKey(srvIPPort, srvPubKey);
            }

            promiseObj->set_value(true);
         }
         else if (server.armoryDBKey.toStdString() != srvPubKey.toHexStr()) {
            ImportKeyBox box(BSMessageBox::question
               , tr("Import ArmoryDB ID Key?")
               , this);

            box.setNewKeyFromBinary(srvPubKey);
            box.setOldKey(server.armoryDBKey);
            box.setAddrPort(srvIPPort);
            box.setCancelVisible(true);

            bool answer = (box.exec() == QDialog::Accepted);

            if (answer) {
               armoryServersProvider_->addKey(srvIPPort, srvPubKey);
            }

            promiseObj->set_value(answer);
         }
         else {
            promiseObj->set_value(true);
         }
      };

      addDeferredDialog(deferredDialog);
   }
   else {
      // server not in the list - added directly to ini config
      promiseObj->set_value(true);
   }
}

void BSTerminalMainWindow::onArmoryNeedsReconnect()
{
   disconnect(statusBarView_.get(), nullptr, nullptr, nullptr);
   statusBarView_->deleteLater();
   QApplication::processEvents();

   initArmory();
   LoadWallets();

   QApplication::processEvents();

   statusBarView_ = std::make_shared<StatusBarView>(armory_, walletsMgr_, assetManager_, celerConnection_
      , signContainer_, ui_->statusbar);

   InitWalletsView();


   InitSigningContainer();
   InitAuthManager();

   connectSigner();
   connectArmory();
}

void BSTerminalMainWindow::onTabWidgetCurrentChanged(const int &index)
{
   const int chatIndex = ui_->tabWidget->indexOf(ui_->widgetChat);
   const bool isChatTab = index == chatIndex;
   //ui_->widgetChat->updateChat(isChatTab);
}

void BSTerminalMainWindow::InitWidgets()
{
   authAddrDlg_ = std::make_shared<AuthAddressDialog>(logMgr_->logger(), authManager_
      , assetManager_, applicationSettings_, this);

   InitWalletsView();
   InitPortfolioView();

   ui_->widgetRFQ->initWidgets(mdProvider_, applicationSettings_);

   auto quoteProvider = std::make_shared<QuoteProvider>(assetManager_, logMgr_->logger("message"));
   quoteProvider->ConnectToCelerClient(celerConnection_);

   autoSignQuoteProvider_ = std::make_shared<AutoSignQuoteProvider>(logMgr_->logger(), assetManager_, quoteProvider
      , applicationSettings_, dealerUtxoAdapter_, signContainer_, mdProvider_, celerConnection_);

   auto dialogManager = std::make_shared<DialogManager>(this);

   ui_->widgetRFQ->init(logMgr_->logger(), celerConnection_, authManager_, quoteProvider, assetManager_
      , dialogManager, signContainer_, armory_, connectionManager_, orderListModel_.get());
   ui_->widgetRFQReply->init(logMgr_->logger(), celerConnection_, authManager_, quoteProvider, mdProvider_, assetManager_
      , applicationSettings_, dialogManager, signContainer_, armory_, connectionManager_, dealerUtxoAdapter_, autoSignQuoteProvider_, orderListModel_.get());

   connect(ui_->widgetRFQ, &RFQRequestWidget::requestPrimaryWalletCreation, this, &BSTerminalMainWindow::onCreatePrimaryWalletRequest);
   connect(ui_->widgetRFQReply, &RFQReplyWidget::requestPrimaryWalletCreation, this, &BSTerminalMainWindow::onCreatePrimaryWalletRequest);

   connect(ui_->tabWidget, &QTabWidget::tabBarClicked, this,
      [requestRFQ = QPointer<RFQRequestWidget>(ui_->widgetRFQ), replyRFQ = QPointer<RFQReplyWidget>(ui_->widgetRFQReply),
         tabWidget = QPointer<QTabWidget>(ui_->tabWidget)](int index) {
      
      if (!tabWidget) {
         return;
      }

      if (requestRFQ && requestRFQ == tabWidget->widget(index)) {
         requestRFQ->forceCheckCondition();
      }

      if (replyRFQ && replyRFQ == tabWidget->widget(index)) {
         replyRFQ->forceCheckCondition();
      }

   });
}

void BSTerminalMainWindow::networkSettingsReceived(const NetworkSettings &settings)
{
   if (!settings.marketData.host.empty()) {
      applicationSettings_->set(ApplicationSettings::mdServerHost, QString::fromStdString(settings.marketData.host));
      applicationSettings_->set(ApplicationSettings::mdServerPort, settings.marketData.port);
   }
   if (!settings.mdhs.host.empty()) {
      applicationSettings_->set(ApplicationSettings::mdhsHost, QString::fromStdString(settings.mdhs.host));
      applicationSettings_->set(ApplicationSettings::mdhsPort, settings.mdhs.port);
   }
#ifndef NDEBUG
   QString chost = applicationSettings_->get<QString>(ApplicationSettings::chatServerHost);
   QString cport = applicationSettings_->get<QString>(ApplicationSettings::chatServerPort);
   if (!settings.chat.host.empty()) {
      if (chost.isEmpty()) {
         applicationSettings_->set(ApplicationSettings::chatServerHost, QString::fromStdString(settings.chat.host));
      }
      if (cport.isEmpty()) {
         applicationSettings_->set(ApplicationSettings::chatServerPort, settings.chat.port);
      }
   }
#else
   if (!settings.chat.host.empty()) {
      applicationSettings_->set(ApplicationSettings::chatServerHost, QString::fromStdString(settings.chat.host));
      applicationSettings_->set(ApplicationSettings::chatServerPort, settings.chat.port);
   }
#endif // NDEBUG

   mdProvider_->SetConnectionSettings(applicationSettings_->get<std::string>(ApplicationSettings::mdServerHost)
      , applicationSettings_->get<std::string>(ApplicationSettings::mdServerPort));
}

void BSTerminalMainWindow::addDeferredDialog(const std::function<void(void)> &deferredDialog)
{
   // multi thread scope, it's safe to call this function from different threads
   QMetaObject::invokeMethod(this, [this, deferredDialog] {
      // single thread scope (main thread), it's safe to push to deferredDialogs_
      // and check deferredDialogRunning_ variable
      deferredDialogs_.push(deferredDialog);
      if(!deferredDialogRunning_) {
         deferredDialogRunning_ = true;
         while (!deferredDialogs_.empty()) {
            deferredDialogs_.front()(); // run stored lambda
            deferredDialogs_.pop();
         }
         deferredDialogRunning_ = false;
      }

   }, Qt::QueuedConnection);
}
