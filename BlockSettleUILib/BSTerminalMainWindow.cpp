/*

***********************************************************************************
* Copyright (C) 2016 - , BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
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
#include "ColoredCoinServer.h"
#include "ConnectionManager.h"
#include "CreateAccountPrompt.h"
#include "CreatePrimaryWalletPrompt.h"
#include "CreateTransactionDialogAdvanced.h"
#include "CreateTransactionDialogSimple.h"
#include "DialogManager.h"
#include "FutureValue.h"
#include "HeadlessContainer.h"
#include "ImportKeyBox.h"
#include "InfoDialogs/AboutDialog.h"
#include "InfoDialogs/MDAgreementDialog.h"
#include "InfoDialogs/StartupDialog.h"
#include "InfoDialogs/SupportDialog.h"
#include "LoginWindow.h"
#include "MDCallbacksQt.h"
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
#include "StatusBarView.h"
#include "SystemFileUtils.h"
#include "TabWithShortcut.h"
#include "TransactionsViewModel.h"
#include "TransactionsWidget.h"
#include "UiUtils.h"
#include "UtxoReservationManager.h"
#include "Wallets/SyncHDWallet.h"
#include "Wallets/SyncWalletsManager.h"

#include "ui_BSTerminalMainWindow.h"

BSTerminalMainWindow::BSTerminalMainWindow(const std::shared_ptr<ApplicationSettings>& settings
   , BSTerminalSplashScreen& splashScreen, QLockFile &lockFile, QWidget* parent)
   : QMainWindow(parent)
   , ui_(new Ui::BSTerminalMainWindow())
   , applicationSettings_(settings)
   , lockFile_(lockFile)
{
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

   splashScreen.show();

   connect(ui_->actionQuit, &QAction::triggered, qApp, &QCoreApplication::quit);

   logMgr_ = std::make_shared<bs::LogManager>();
   logMgr_->add(applicationSettings_->GetLogsConfig());

   logMgr_->logger()->debug("Settings loaded from {}", applicationSettings_->GetSettingsPath().toStdString());

   bs::UtxoReservation::init(logMgr_->logger());

   setupIcon();
   UiUtils::setupIconFont(this);
   NotificationCenter::createInstance(logMgr_->logger(), applicationSettings_, ui_.get(), sysTrayIcon_, this);

   cbApprovePuB_ = PubKeyLoader::getApprovingCallback(PubKeyLoader::KeyType::PublicBridge
      , this, applicationSettings_);
   cbApproveChat_ = PubKeyLoader::getApprovingCallback(PubKeyLoader::KeyType::Chat
      , this, applicationSettings_);
   cbApproveProxy_ = PubKeyLoader::getApprovingCallback(PubKeyLoader::KeyType::Proxy
      , this, applicationSettings_);
   cbApproveCcServer_ = PubKeyLoader::getApprovingCallback(PubKeyLoader::KeyType::CcServer
      , this, applicationSettings_);

   initConnections();
   initArmory();
   initCcClient();

   walletsMgr_ = std::make_shared<bs::sync::WalletsManager>(logMgr_->logger(), applicationSettings_, armory_, trackerClient_);

   if (!applicationSettings_->get<bool>(ApplicationSettings::initialized)) {
      applicationSettings_->SetDefaultSettings(true);
   }

   InitAssets();
   InitSigningContainer();
   InitAuthManager();
   initUtxoReservationManager();

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
   connectCcClient();

   InitChartsView();

   ui_->tabWidget->setCurrentIndex(settings->get<int>(ApplicationSettings::GUI_main_tab));

   ui_->widgetTransactions->setAppSettings(applicationSettings_);

   UpdateMainWindowAppearence();
   setWidgetsAuthorized(false);

   updateControlEnabledState();

   InitWidgets();
}

void BSTerminalMainWindow::onNetworkSettingsRequired(NetworkSettingsClient client)
{
   networkSettingsLoader_ = std::make_unique<NetworkSettingsLoader>(logMgr_->logger()
      , applicationSettings_->pubBridgeHost(), applicationSettings_->pubBridgePort(), cbApprovePuB_);

   connect(networkSettingsLoader_.get(), &NetworkSettingsLoader::succeed, this, [this, client] {
      networkSettingsReceived(networkSettingsLoader_->settings(), client);
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

void BSTerminalMainWindow::onInitWalletDialogWasShown()
{
   initialWalletCreateDialogShown_ = true;
}

void BSTerminalMainWindow::onAddrStateChanged()
{
   bool canSubmitAuthAddr = (userType_ == bs::network::UserType::Trading)
         || (userType_ == bs::network::UserType::Dealing);

   if (allowAuthAddressDialogShow_ && authManager_ && authManager_->HasAuthAddr() && authManager_->isAllLoadded()
      && !authManager_->isAtLeastOneAwaitingVerification() && canSubmitAuthAddr) {
      allowAuthAddressDialogShow_ = false;
      BSMessageBox qry(BSMessageBox::question, tr("Authentication Address"), tr("Authentication Address")
         , tr("Trading and settlement of XBT products require an Authentication Address to validate you as a Participant of BlockSettle’s Trading Network.\n"
              "\n"
              "Submit Authentication Address now?"), this);
      if (qry.exec() == QDialog::Accepted) {
         openAuthManagerDialog();
      }
   }

   if (authManager_->isAtLeastOneAwaitingVerification()) {
      allowAuthAddressDialogShow_ = false;
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

void BSTerminalMainWindow::loadPositionAndShow()
{
   auto geom = applicationSettings_->get<QRect>(ApplicationSettings::GUI_main_geometry);
   if (geom.isEmpty()) {
      show();
      return;
   }
   setGeometry(geom);   // This call is required for screenNumber() method to work properly

#ifdef Q_OS_WINDOWS
   int screenNo = QApplication::desktop()->screenNumber(this);
   if (screenNo < 0) {
      screenNo = 0;
   }
   const auto screenGeom = QApplication::desktop()->screenGeometry(screenNo);
   if (!screenGeom.contains(geom)) {
      const int screenWidth = screenGeom.width() * 0.9;
      const int screenHeight = screenGeom.height() * 0.9;
      geom.setWidth(std::min(geom.width(), screenWidth));
      geom.setHeight(std::min(geom.height(), screenHeight));
      geom.moveCenter(screenGeom.center());
   }
   const auto screen = qApp->screens()[screenNo];
   const float pixelRatio = screen->devicePixelRatio();
   if (pixelRatio > 1.0) {
      const float coeff = 0.9999;   // some coefficient that prevents oversizing of main window on HiRes display on Windows
      geom.setWidth(geom.width() * coeff);
      geom.setHeight(geom.height() * coeff);
   }
   setGeometry(geom);
#else
   if (QApplication::desktop()->screenNumber(this) == -1) {
      auto currentScreenRect = QApplication::desktop()->screenGeometry(QCursor::pos());
      // Do not delete 0.9 multiplier, since in some system window size is applying without system native toolbar
      geom.setWidth(std::min(geom.width(), static_cast<int>(currentScreenRect.width() * 0.9)));
      geom.setHeight(std::min(geom.height(), static_cast<int>(currentScreenRect.height() * 0.9)));
      geom.moveCenter(currentScreenRect.center());
      setGeometry(geom);
}
#endif   // not Windows

   show();
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
   // Check UTXO reservation state before any other destructors call!
   if (bs::UtxoReservation::instance()) {
      bs::UtxoReservation::instance()->shutdownCheck();
   }

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
}

void BSTerminalMainWindow::setupToolbar()
{
   action_send_ = new QAction(tr("Create &Transaction"), this);
   connect(action_send_, &QAction::triggered, this, &BSTerminalMainWindow::onSend);

   action_generate_address_ = new QAction(tr("Generate &Address"), this);
   connect(action_generate_address_, &QAction::triggered, this, &BSTerminalMainWindow::onGenerateAddress);

   action_login_ = new QAction(tr("Login to BlockSettle"), this);
   connect(action_login_, &QAction::triggered, this, &BSTerminalMainWindow::onLogin);

   action_logout_ = new QAction(tr("Logout from BlockSettle"), this);
   connect(action_logout_, &QAction::triggered, this, &BSTerminalMainWindow::onLogout);

   setupTopRightWidget();

   action_logout_->setVisible(false);

   connect(ui_->pushButtonUser, &QPushButton::clicked, this, &BSTerminalMainWindow::onButtonUserClicked);

   QMenu* trayMenu = new QMenu(this);
   QAction* trayShowAction = trayMenu->addAction(tr("&Open Terminal"));
   connect(trayShowAction, &QAction::triggered, this, &QMainWindow::show);
   trayMenu->addSeparator();

   trayMenu->addAction(action_send_);
   trayMenu->addAction(action_generate_address_);
   trayMenu->addAction(ui_->actionSettings);

   trayMenu->addSeparator();
   trayMenu->addAction(ui_->actionQuit);
   sysTrayIcon_->setContextMenu(trayMenu);
}

void BSTerminalMainWindow::setupTopRightWidget()
{
   auto toolBar = new QToolBar(this);
   toolBar->setObjectName(QLatin1String("mainToolBar"));
   toolBar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
   ui_->tabWidget->setCornerWidget(toolBar, Qt::TopRightCorner);

   toolBar->addAction(action_send_);
   toolBar->addAction(action_generate_address_);

   for (int i = 0; i < toolBar->children().size(); ++i) {
      auto *toolButton = qobject_cast<QToolButton*>(toolBar->children().at(i));
      if (toolButton && (toolButton->defaultAction() == action_send_
         || toolButton->defaultAction() == action_generate_address_)) {
         toolButton->setObjectName(QLatin1String("mainToolBarActions"));
      }
   }

#ifdef Q_OS_WIN
   ui_->tabWidget->setProperty("onWindows", QVariant(true));
#else
   ui_->tabWidget->setProperty("onLinux", QVariant(true));
#endif // DEBUG
   auto *prevStyle = ui_->tabWidget->style();
   ui_->tabWidget->setStyle(nullptr);
   ui_->tabWidget->setStyle(prevStyle);
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

   mdCallbacks_ = std::make_shared<MDCallbacksQt>();
   mdProvider_ = std::make_shared<BSMarketDataProvider>(connectionManager_
      , logMgr_->logger("message"), mdCallbacks_.get());
   connect(mdCallbacks_.get(), &MDCallbacksQt::UserWantToConnectToMD, this, &BSTerminalMainWindow::acceptMDAgreement);
   connect(mdCallbacks_.get(), &MDCallbacksQt::WaitingForConnectionDetails, this, [this] {
      onNetworkSettingsRequired(NetworkSettingsClient::MarketData);
   });
}

void BSTerminalMainWindow::LoadWallets()
{
   logMgr_->logger()->debug("Loading wallets");

   connect(walletsMgr_.get(), &bs::sync::WalletsManager::walletsReady, this, [this] {
      ui_->widgetRFQ->setWalletsManager(walletsMgr_);
      ui_->widgetRFQReply->setWalletsManager(walletsMgr_);
      autoSignQuoteProvider_->setWalletsManager(walletsMgr_);
   });
   connect(walletsMgr_.get(), &bs::sync::WalletsManager::walletsSynchronized, this, [this] {
      walletsSynched_ = true;
      updateControlEnabledState();
      CompleteDBConnection();
      act_->onRefresh({}, true);
      tryGetChatKeys();

      // BST-2645: Do not show create account prompt if already have wallets
      if (walletsMgr_->walletsCount() > 0) {
         disableCreateTestAccountPrompt();
      }
   });
   connect(walletsMgr_.get(), &bs::sync::WalletsManager::info, this, &BSTerminalMainWindow::showInfo);
   connect(walletsMgr_.get(), &bs::sync::WalletsManager::error, this, &BSTerminalMainWindow::showError);

   // Enable/disable send action when first wallet created/last wallet removed
   connect(walletsMgr_.get(), &bs::sync::WalletsManager::walletChanged, this
      , &BSTerminalMainWindow::updateControlEnabledState);
   connect(walletsMgr_.get(), &bs::sync::WalletsManager::walletDeleted, this, [this] {
      updateControlEnabledState();
      resetChatKeys();
   });
   connect(walletsMgr_.get(), &bs::sync::WalletsManager::walletAdded, this, [this] {
      updateControlEnabledState();
      promptToCreateTestAccountIfNeeded();
      tryGetChatKeys();
   });
   connect(walletsMgr_.get(), &bs::sync::WalletsManager::newWalletAdded, this
      , &BSTerminalMainWindow::updateControlEnabledState);

   connect(walletsMgr_.get(), &bs::sync::WalletsManager::walletBalanceUpdated, this, [this](const std::string &walletId) {
      auto wallet = dynamic_cast<bs::sync::hd::Leaf*>(walletsMgr_->getWalletById(walletId).get());
      if (wallet && wallet->purpose() == bs::hd::Purpose::NonSegWit && wallet->getTotalBalance() > 0) {
         showLegacyWarningIfNeeded();
      }
   });

   onSyncWallets();
}

void BSTerminalMainWindow::InitAuthManager()
{
   authManager_ = std::make_shared<AuthAddressManager>(logMgr_->logger(), armory_);
   authManager_->init(applicationSettings_, walletsMgr_, signContainer_);

   connect(authManager_.get(), &AuthAddressManager::AddrVerifiedOrRevoked, this, [](const QString &addr, const QString &state) {
      NotificationCenter::notify(bs::ui::NotifyType::AuthAddress, { addr, state });
   });
   connect(authManager_.get(), &AuthAddressManager::AddrStateChanged, this, &BSTerminalMainWindow::onAddrStateChanged, Qt::QueuedConnection);
   connect(authManager_.get(), &AuthAddressManager::AuthWalletCreated, this, [this](const QString &walletId) {
      if (authAddrDlg_ && walletId.isEmpty()) {
         openAuthManagerDialog();
      }
   });
   connect(authManager_.get(), &AuthAddressManager::ConnectionComplete, this, [this]() {
      if (!authManager_->HaveAuthWallet() && !createAuthWalletDialogShown_ && !promoteToPrimaryShown_) {
         createAuthWalletDialogShown_ = true;
         createAuthWallet(nullptr);
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

std::shared_ptr<WalletSignerContainer> BSTerminalMainWindow::createRemoteSigner(bool restoreHeadless)
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

   if (restoreHeadless) {
      // setup headleass signer back (it was changed when createLocalSigner called signersProvider_->switchToLocalFullGUI)
      signersProvider_->setupSigner(0, true);
   }

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
      return createRemoteSigner(true);
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
   signContainer_ = createSigner();

   if (!signContainer_) {
      showError(tr("BlockSettle Signer"), tr("BlockSettle Signer creation failure"));
      return false;
   }
   connect(signContainer_.get(), &SignContainer::connectionError, this, &BSTerminalMainWindow::onSignerConnError, Qt::QueuedConnection);
   connect(signContainer_.get(), &SignContainer::disconnected, this, &BSTerminalMainWindow::updateControlEnabledState, Qt::QueuedConnection);

   walletsMgr_->setSignContainer(signContainer_);
   connect(signContainer_.get(), &WalletSignerContainer::ready, this, &BSTerminalMainWindow::SignerReady, Qt::QueuedConnection);
   connect(signContainer_.get(), &WalletSignerContainer::needNewWalletPrompt, this, &BSTerminalMainWindow::onNeedNewWallet, Qt::QueuedConnection);
   connect(signContainer_.get(), &WalletSignerContainer::walletsReadyToSync, this, &BSTerminalMainWindow::onSyncWallets, Qt::QueuedConnection);
   connect(signContainer_.get(), &WalletSignerContainer::windowVisibilityChanged, this, &BSTerminalMainWindow::onSignerVisibleChanged, Qt::QueuedConnection);

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

void BSTerminalMainWindow::onNeedNewWallet()
{
   if (!initialWalletCreateDialogShown_) {
      initialWalletCreateDialogShown_ = true;
      const auto &deferredDialog = [this]{
         createWallet(true);
      };
      addDeferredDialog(deferredDialog);
   }
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
      action_send_->setEnabled(!walletsMgr_->hdWallets().empty()
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
   startupDialog.init(applicationSettings_);
   int result = startupDialog.exec();

   if (result == QDialog::Rejected) {
      hide();
      return false;
   }

   // Need update armory settings if case user selects TestNet
   startupDialog.applySelectedConnectivity(armoryServersProvider_);
   applicationSettings_->selectNetwork();

   return true;
}

void BSTerminalMainWindow::InitAssets()
{
   ccFileManager_ = std::make_shared<CCFileManager>(logMgr_->logger(), applicationSettings_);
   assetManager_ = std::make_shared<AssetManager>(logMgr_->logger(), walletsMgr_
      , mdCallbacks_, celerConnection_);
   assetManager_->init();

   orderListModel_ = std::make_unique<OrderListModel>(assetManager_);

   connect(ccFileManager_.get(), &CCFileManager::CCSecurityDef, assetManager_.get(), &AssetManager::onCCSecurityReceived);
   connect(ccFileManager_.get(), &CCFileManager::CCSecurityInfo, walletsMgr_.get(), &bs::sync::WalletsManager::onCCSecurityInfo);
   connect(ccFileManager_.get(), &CCFileManager::Loaded, this, &BSTerminalMainWindow::onCCLoaded);
   connect(ccFileManager_.get(), &CCFileManager::LoadingFailed, this, &BSTerminalMainWindow::onCCInfoMissing);
   connect(ccFileManager_.get(), &CCFileManager::definitionsLoadedFromPub, this, &BSTerminalMainWindow::onCcDefinitionsLoadedFromPub);

   connect(mdCallbacks_.get(), &MDCallbacksQt::MDUpdate, assetManager_.get(), &AssetManager::onMDUpdate);

   if (ccFileManager_->hasLocalFile()) {
      ccFileManager_->LoadSavedCCDefinitions();
   }
}

void BSTerminalMainWindow::InitPortfolioView()
{
   portfolioModel_ = std::make_shared<CCPortfolioModel>(walletsMgr_, assetManager_, this);
   ui_->widgetPortfolio->init(applicationSettings_, mdProvider_, mdCallbacks_
      , portfolioModel_, signContainer_, armory_, utxoReservationMgr_, logMgr_->logger("ui"), walletsMgr_);
}

void BSTerminalMainWindow::InitWalletsView()
{
   ui_->widgetWallets->init(logMgr_->logger("ui"), walletsMgr_, signContainer_
      , applicationSettings_, connectionManager_, assetManager_, authManager_, armory_);
   connect(ui_->widgetWallets, &WalletsWidget::newWalletCreationRequest, this, &BSTerminalMainWindow::onInitWalletDialogWasShown);
}

void BSTerminalMainWindow::tryInitChatView()
{
   // Chat initialization is a bit convoluted.
   // First we need to create and initialize chatClientServicePtr_ (which lives in background thread and so is async).
   // For this it needs to know chat server address where to connect and chat keys used for chat messages encryption.
   // Only after that we could init ui_->widgetChat and try to login after that.
   if (chatInitState_ != ChatInitState::NoStarted || !networkSettingsReceived_ || !gotChatKeys_) {
      return;
   }
   chatInitState_ = ChatInitState::InProgress;

   chatClientServicePtr_ = std::make_shared<Chat::ChatClientService>();

   connect(chatClientServicePtr_.get(), &Chat::ChatClientService::initDone, this, [this]() {
      const bool isProd = applicationSettings_->get<int>(ApplicationSettings::envConfiguration) ==
         static_cast<int>(ApplicationSettings::EnvConfiguration::Production);
      const auto env = isProd ? bs::network::otc::Env::Prod : bs::network::otc::Env::Test;

      ui_->widgetChat->init(connectionManager_, env, chatClientServicePtr_,
         logMgr_->logger("chat"), walletsMgr_, authManager_, armory_, signContainer_, mdCallbacks_, assetManager_, utxoReservationMgr_);

      connect(chatClientServicePtr_->getClientPartyModelPtr().get(), &Chat::ClientPartyModel::userPublicKeyChanged,
         this, [this](const Chat::UserPublicKeyInfoList& userPublicKeyInfoList) {
         addDeferredDialog([this, userPublicKeyList = userPublicKeyInfoList]() {
            ui_->widgetChat->onUserPublicKeyChanged(userPublicKeyList);
         });
      }, Qt::QueuedConnection);

      chatInitState_ = ChatInitState::Done;
      tryLoginIntoChat();
   });

   Chat::ChatSettings chatSettings;
   chatSettings.connectionManager = connectionManager_;

   chatSettings.chatPrivKey = chatPrivKey_;
   chatSettings.chatPubKey = chatPubKey_;

   chatSettings.chatServerHost = applicationSettings_->get<std::string>(ApplicationSettings::chatServerHost);
   chatSettings.chatServerPort = applicationSettings_->get<std::string>(ApplicationSettings::chatServerPort);
   chatSettings.chatDbFile = applicationSettings_->get<QString>(ApplicationSettings::chatDbFile);

   chatClientServicePtr_->Init(logMgr_->logger("chat"), chatSettings);

   connect(ui_->tabWidget, &QTabWidget::currentChanged, this, &BSTerminalMainWindow::onTabWidgetCurrentChanged);
   connect(ui_->widgetChat, &ChatWidget::requestPrimaryWalletCreation, this, &BSTerminalMainWindow::onCreatePrimaryWalletRequest);

   if (NotificationCenter::instance() != nullptr) {
      connect(NotificationCenter::instance(), &NotificationCenter::newChatMessageClick, ui_->widgetChat, &ChatWidget::onNewChatMessageTrayNotificationClicked);
   }
}

void BSTerminalMainWindow::tryLoginIntoChat()
{
   if (chatInitState_ != ChatInitState::Done || chatTokenData_.empty() || chatTokenSign_.empty()) {
      return;
   }

   chatClientServicePtr_->LoginToServer(chatTokenData_, chatTokenSign_, cbApproveChat_);

   chatTokenData_.clear();
   chatTokenSign_.clear();
}

void BSTerminalMainWindow::resetChatKeys()
{
   gotChatKeys_ = false;
   chatPubKey_.clear();
   chatPrivKey_.clear();
   tryGetChatKeys();
}

void BSTerminalMainWindow::tryGetChatKeys()
{
   if (gotChatKeys_) {
      return;
   }
   const auto &primaryWallet = walletsMgr_->getPrimaryWallet();
   if (!primaryWallet) {
      return;
   }
   signContainer_->getChatNode(primaryWallet->walletId(), [this](const BIP32_Node &node) {
      if (node.getPublicKey().empty() || node.getPrivateKey().empty()) {
         SPDLOG_LOGGER_ERROR(logMgr_->logger(), "chat keys is empty");
         return;
      }
      chatPubKey_ = node.getPublicKey();
      chatPrivKey_ = node.getPrivateKey();
      gotChatKeys_ = true;
      tryInitChatView();
   });
}

void BSTerminalMainWindow::InitChartsView()
{
   ui_->widgetChart->init(applicationSettings_, mdProvider_, mdCallbacks_
      , connectionManager_, logMgr_->logger("ui"));
}

// Initialize widgets related to transactions.
void BSTerminalMainWindow::InitTransactionsView()
{
   ui_->widgetExplorer->init(armory_, logMgr_->logger(), walletsMgr_, ccFileManager_, authManager_);
   ui_->widgetTransactions->init(walletsMgr_, armory_, utxoReservationMgr_, signContainer_,
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

         parent_->armory_->getNodeStatus([this] (const std::shared_ptr<ClientClasses::NodeStatusStruct> &status){
            QMetaObject::invokeMethod(parent_, [this, status] {
               if (status) {
                  parent_->onNodeStatus(status->status(), status->isSegWitEnabled(), status->rpcStatus());
               }
            });
         });
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

void BSTerminalMainWindow::initCcClient()
{
   bool isDefaultArmory = armoryServersProvider_->isDefault(armoryServersProvider_->indexOfCurrent());
   if (isDefaultArmory) {
      trackerClient_ = std::make_shared<CcTrackerClient>(logMgr_->logger());
   }
}

void BSTerminalMainWindow::initUtxoReservationManager()
{
   utxoReservationMgr_ = std::make_shared<bs::UTXOReservationManager>(
      walletsMgr_, armory_, logMgr_->logger());
}

void BSTerminalMainWindow::MainWinACT::onTxBroadcastError(const BinaryData &txHash, int errCode
   , const std::string &errMsg)
{
   NotificationCenter::notify(bs::ui::NotifyType::BroadcastError,
      { QString::fromStdString(txHash.toHexStr(true)), QString::fromStdString(errMsg) });
}

void BSTerminalMainWindow::MainWinACT::onNodeStatus(NodeStatus nodeStatus, bool isSegWitEnabled, RpcStatus rpcStatus)
{
   QMetaObject::invokeMethod(parent_, [parent = parent_, nodeStatus, isSegWitEnabled, rpcStatus] {
      parent->onNodeStatus(nodeStatus, isSegWitEnabled, rpcStatus);
   });
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

void BSTerminalMainWindow::connectCcClient()
{
   if (trackerClient_) {
      bool testnet = applicationSettings_->get<NetworkType>(ApplicationSettings::netType) == NetworkType::TestNet;
      trackerClient_->openConnection("185.213.153.37", testnet ? "19003" : "9003", cbApproveCcServer_);
   }
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

bool BSTerminalMainWindow::createWallet(bool primary, const std::function<void()> &cb)
{
   if (primary) {
      auto primaryWallet = walletsMgr_->getPrimaryWallet();
      if (primaryWallet) {
         if (cb) {
            cb();
         }
         return true;
      }

      const auto &hdWallets = walletsMgr_->hdWallets();
      const auto fullWalletIt = std::find_if(hdWallets.begin(), hdWallets.end(), [](const std::shared_ptr<bs::sync::hd::Wallet> &wallet) {
         return !wallet->isHardwareWallet() && !wallet->isOffline();
      });
      if (fullWalletIt != hdWallets.end()) {
         auto wallet = *fullWalletIt;
         promoteToPrimaryShown_ = true;
         BSMessageBox qry(BSMessageBox::question, tr("Promote to primary wallet"), tr("Promote to primary wallet?")
            , tr("To trade through BlockSettle, you are required to have a wallet which"
               " supports the sub-wallets required to interact with the system. Each Terminal"
               " may only have one Primary Wallet. Do you wish to promote '%1'?")
            .arg(QString::fromStdString(wallet->name())), this);
         if (qry.exec() == QDialog::Accepted) {
            walletsMgr_->PromoteHDWallet(wallet->walletId(), [this, cb](bs::error::ErrorCode result) {
               if (result == bs::error::ErrorCode::NoError) {
                  if (cb) {
                     cb();
                  }
                  // If wallet was promoted to primary we could try to get chat keys now
                  tryGetChatKeys();
               }
            });
            return true;
         }
         return false;
      }
   }

   if (!signContainer_->isOffline()) {
      NewWalletDialog newWalletDialog(true, applicationSettings_, this);
      onInitWalletDialogWasShown();

      int rc = newWalletDialog.exec();

      switch (rc) {
         case NewWalletDialog::CreateNew:
            ui_->widgetWallets->CreateNewWallet();
            break;
         case NewWalletDialog::ImportExisting:
            ui_->widgetWallets->ImportNewWallet();
            break;
         case NewWalletDialog::ImportHw:
            ui_->widgetWallets->ImportHwWallet();
            break;
         case NewWalletDialog::Cancel:
            return false;
      }

      if (cb) {
         cb();
      }
      return true;
   } else {
      ui_->widgetWallets->ImportNewWallet();
      if (cb) {
         cb();
      }
   }

   return true;
}

void BSTerminalMainWindow::onCreatePrimaryWalletRequest()
{
   bool result = createWallet(true);

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

void BSTerminalMainWindow::onGenerateAddress()
{
   if (walletsMgr_->hdWallets().empty()) {
      createWallet(true);
      return;
   }

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
   SelectWalletDialog selectWalletDialog(walletsMgr_, selWalletId, this);
   selectWalletDialog.exec();

   if (selectWalletDialog.result() == QDialog::Rejected) {
      return;
   }

   NewAddressDialog newAddressDialog(selectWalletDialog.getSelectedWallet(), this);
   newAddressDialog.exec();
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


   std::shared_ptr<CreateTransactionDialog> dlg;

   if ((QGuiApplication::keyboardModifiers() & Qt::ShiftModifier)
       || applicationSettings_->get<bool>(ApplicationSettings::AdvancedTxDialogByDefault)) {
      dlg = std::make_shared<CreateTransactionDialogAdvanced>(armory_, walletsMgr_, utxoReservationMgr_
         , signContainer_, true, logMgr_->logger("ui"), applicationSettings_, nullptr, bs::UtxoReservationToken{}, this );
   } else {
      dlg = std::make_shared<CreateTransactionDialogSimple>(armory_, walletsMgr_, utxoReservationMgr_, signContainer_
            , logMgr_->logger("ui"), applicationSettings_, this);
   }

   if (!selectedWalletId.empty()) {
      dlg->SelectWallet(selectedWalletId);
   }

   while(true) {
      dlg->exec();

      if  ((dlg->result() != QDialog::Accepted) || !dlg->switchModeRequested()) {
         break;
      }

      auto nextDialog = dlg->SwithcMode();
      dlg = nextDialog;
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

   AboutDialog *aboutDlg = new AboutDialog(applicationSettings_->get<QString>(ApplicationSettings::ChangeLog_Base_Url), this);
   auto aboutDlgCb = [aboutDlg] (int tab) {
      return [aboutDlg, tab]() {
         aboutDlg->setTab(tab);
         aboutDlg->show();
      };
   };

   SupportDialog *supportDlg = new SupportDialog(this);
   auto supportDlgCb = [supportDlg] (int tab) {
      return [supportDlg, tab]() {
         supportDlg->setTab(tab);
         supportDlg->show();
      };
   };

   connect(ui_->actionCreateNewWallet, &QAction::triggered, this, [ww = ui_->widgetWallets]{ ww->onNewWallet(); });
   connect(ui_->actionAuthenticationAddresses, &QAction::triggered, this, &BSTerminalMainWindow::openAuthManagerDialog);
   connect(ui_->actionSettings, &QAction::triggered, this, [=]() { openConfigDialog(); });
   connect(ui_->actionAccountInformation, &QAction::triggered, this, &BSTerminalMainWindow::openAccountInfoDialog);
   connect(ui_->actionEnterColorCoinToken, &QAction::triggered, this, &BSTerminalMainWindow::openCCTokenDialog);
   connect(ui_->actionAbout, &QAction::triggered, aboutDlgCb(0));
   connect(ui_->actionVersion, &QAction::triggered, aboutDlgCb(3));
   connect(ui_->actionGuides, &QAction::triggered, supportDlgCb(0));
   connect(ui_->actionContact, &QAction::triggered, supportDlgCb(1));

   onUserLoggedOut();

#ifndef Q_OS_MAC
   ui_->horizontalFrame->hide();
   ui_->menubar->setCornerWidget(ui_->loginGroupWidget);
#endif

   auto envType = static_cast<ApplicationSettings::EnvConfiguration>(applicationSettings_->get(ApplicationSettings::envConfiguration).toInt());
   bool isProd = envType == ApplicationSettings::EnvConfiguration::Production;
   ui_->prodEnvSettings->setEnabled(!isProd);
   ui_->testEnvSettings->setEnabled(isProd);
   connect(ui_->prodEnvSettings, &QPushButton::clicked, this, [this] {
      promptSwitchEnv(true);
   });
   connect(ui_->testEnvSettings, &QPushButton::clicked, this, [this] {
      promptSwitchEnv(false);
   });
}

void BSTerminalMainWindow::openAuthManagerDialog()
{
   allowAuthAddressDialogShow_ = false;
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

void BSTerminalMainWindow::openConfigDialog(bool showInNetworkPage)
{
   ConfigDialog configDialog(applicationSettings_, armoryServersProvider_, signersProvider_, signContainer_, this);
   connect(&configDialog, &ConfigDialog::reconnectArmory, this, &BSTerminalMainWindow::onArmoryNeedsReconnect);

   if (showInNetworkPage) {
      configDialog.popupNetworkSettings();
   }

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
         CCTokenEntryDialog(walletsMgr_, ccFileManager_, applicationSettings_, this).exec();
      });
   };
   // Do not use deferredDialogs_ here as it will deadblock PuB public key processing
   if (walletsMgr_->hasPrimaryWallet()) {
      lbdCCTokenDlg();
   }
   else {
      createWallet(true, lbdCCTokenDlg);
   }
}

void BSTerminalMainWindow::onLogin()
{
   onNetworkSettingsRequired(NetworkSettingsClient::Login);
}

void BSTerminalMainWindow::onLoginProceed(const NetworkSettings &networkSettings)
{
   if (networkSettings.status == Blocksettle::Communication::GetNetworkSettingsResponse_Status_LIVE_TRADING_COMING_SOON) {
      BSMessageBox mbox(BSMessageBox::question, tr("Login to BlockSettle"), tr("Live trading is coming soon...")
                   , tr("In the meantime, you can try p2p trading in our testnet environment. Would you like to do so now?"), this);
      mbox.setCancelButtonText(tr("Cancel"));
      mbox.setConfirmButtonText(tr("Yes"));
      int rc = mbox.exec();
      if (rc == QDialog::Accepted) {
         switchToTestEnv();
         restartTerminal();
      }
      return;
   }

   if (!gotChatKeys_) {
      addDeferredDialog([this] {
         CreatePrimaryWalletPrompt dlg;
         int rc = dlg.exec();
         if (rc == CreatePrimaryWalletPrompt::CreateWallet) {
            ui_->widgetWallets->CreateNewWallet();
         } else if (rc == CreatePrimaryWalletPrompt::ImportWallet) {
            ui_->widgetWallets->ImportNewWallet();
         }
      });
      return;
   }

   if (!gotChatKeys_) {
      if (!signContainer_ || !signContainer_->isReady()) {
         BSMessageBox signerMsg(BSMessageBox::warning
            , tr("Login Failed")
            , tr("Login Failed")
            , tr("Signer connection lost. Please reconnect and try to login again.")
            , this);
         signerMsg.exec();
         return;
      }
   }

   LoginWindow loginDialog(logMgr_->logger("autheID"), applicationSettings_, &cbApprovePuB_, &cbApproveProxy_, this);

   int rc = loginDialog.exec();

   if (rc != QDialog::Accepted && !loginDialog.result()) {
      return;
   }

   bool isRegistered = (loginDialog.result()->userType == bs::network::UserType::Market
      || loginDialog.result()->userType == bs::network::UserType::Trading
      || loginDialog.result()->userType == bs::network::UserType::Dealing);
   auto envType = static_cast<ApplicationSettings::EnvConfiguration>(applicationSettings_->get(ApplicationSettings::envConfiguration).toInt());
   if (!isRegistered && envType == ApplicationSettings::EnvConfiguration::Test) {
      auto createTestAccountUrl = applicationSettings_->get<QString>(ApplicationSettings::GetAccount_UrlTest);
      BSMessageBox dlg(BSMessageBox::info, tr("Create Test Account")
         , tr("Create a BlockSettle test account")
         , tr("<p>Login requires a test account - create one in minutes on test.blocksettle.com</p>"
              "<p>Once you have registered, return to login in the Terminal.</p>"
              "<a href=\"%1\"><span style=\"text-decoration: underline;color:%2;\">Create Test Account Now</span></a>")
         .arg(createTestAccountUrl).arg(BSMessageBox::kUrlColor), this);
      dlg.setOkVisible(false);
      dlg.setCancelVisible(true);
      dlg.exec();
      return;
   }

   currentUserLogin_ = loginDialog.email();

   networkSettingsReceived(loginDialog.networkSettings(), NetworkSettingsClient::MarketData);

   chatTokenData_ = loginDialog.result()->chatTokenData;
   chatTokenSign_ = loginDialog.result()->chatTokenSign;
   tryLoginIntoChat();

   bsClient_ = loginDialog.getClient();
   ccFileManager_->setBsClient(bsClient_.get());
   authAddrDlg_->setBsClient(bsClient_.get());

   ccFileManager_->setCcAddressesSigned(loginDialog.result()->ccAddressesSigned);
   authManager_->setAuthAddressesSigned(loginDialog.result()->authAddressesSigned);

   connect(bsClient_.get(), &BsClient::disconnected, orderListModel_.get(), &OrderListModel::onDisconnected);
   connect(bsClient_.get(), &BsClient::connectionFailed, this, &BSTerminalMainWindow::onBsConnectionFailed);

   // connect to RFQ dialog
   connect(bsClient_.get(), &BsClient::processPbMessage, ui_->widgetRFQ, &RFQRequestWidget::onMessageFromPB);
   connect(ui_->widgetRFQ, &RFQRequestWidget::sendUnsignedPayinToPB, bsClient_.get(), &BsClient::sendUnsignedPayin);
   connect(ui_->widgetRFQ, &RFQRequestWidget::sendSignedPayinToPB, bsClient_.get(), &BsClient::sendSignedPayin);
   connect(ui_->widgetRFQ, &RFQRequestWidget::sendSignedPayoutToPB, bsClient_.get(), &BsClient::sendSignedPayout);

   connect(ui_->widgetRFQ, &RFQRequestWidget::cancelXBTTrade, bsClient_.get(), &BsClient::sendCancelOnXBTTrade);
   connect(ui_->widgetRFQ, &RFQRequestWidget::cancelCCTrade, bsClient_.get(), &BsClient::sendCancelOnCCTrade);

   // connect to quote dialog
   connect(bsClient_.get(), &BsClient::processPbMessage, ui_->widgetRFQReply, &RFQReplyWidget::onMessageFromPB);
   connect(ui_->widgetRFQReply, &RFQReplyWidget::sendUnsignedPayinToPB, bsClient_.get(), &BsClient::sendUnsignedPayin);
   connect(ui_->widgetRFQReply, &RFQReplyWidget::sendSignedPayinToPB, bsClient_.get(), &BsClient::sendSignedPayin);
   connect(ui_->widgetRFQReply, &RFQReplyWidget::sendSignedPayoutToPB, bsClient_.get(), &BsClient::sendSignedPayout);

   connect(ui_->widgetRFQReply, &RFQReplyWidget::cancelXBTTrade, bsClient_.get(), &BsClient::sendCancelOnXBTTrade);
   connect(ui_->widgetRFQReply, &RFQReplyWidget::cancelCCTrade, bsClient_.get(), &BsClient::sendCancelOnCCTrade);

   connect(ui_->widgetChat, &ChatWidget::emailHashRequested, bsClient_.get(), &BsClient::findEmailHash);
   connect(bsClient_.get(), &BsClient::emailHashReceived, ui_->widgetChat, &ChatWidget::onEmailHashReceived);

   connect(bsClient_.get(), &BsClient::processPbMessage, orderListModel_.get(), &OrderListModel::onMessageFromPB);

   utxoReservationMgr_->setFeeRatePb(loginDialog.result()->feeRatePb);
   connect(bsClient_.get(), &BsClient::feeRateReceived, this, [this] (float feeRate) {
      utxoReservationMgr_->setFeeRatePb(feeRate);
   });

   authManager_->setCelerClient(celerConnection_);

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

   connect(bsClient_.get(), &BsClient::processPbMessage, ui_->widgetChat, &ChatWidget::onProcessOtcPbMessage);
   connect(ui_->widgetChat, &ChatWidget::sendOtcPbMessage, bsClient_.get(), &BsClient::sendPbMessage);

   connect(bsClient_.get(), &BsClient::ccGenAddrUpdated, this, [this](const BinaryData &ccGenAddrData) {
      ccFileManager_->setCcAddressesSigned(ccGenAddrData);
   });

   accountEnabled_ = true;
   onAccountTypeChanged(loginDialog.result()->userType, loginDialog.result()->enabled);
   connect(bsClient_.get(), &BsClient::accountStateChanged, this, [this](bs::network::UserType userType, bool enabled) {
      onAccountTypeChanged(userType, enabled);
   });
}

void BSTerminalMainWindow::onLogout()
{
   ui_->widgetWallets->setUsername(QString());
   if (chatClientServicePtr_) {
      chatClientServicePtr_->LogoutFromServer();
   }
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

   ccFileManager_->ConnectToCelerClient(celerConnection_);

   const auto userId = BinaryData::CreateFromHex(celerConnection_->userId());
   const auto &deferredDialog = [this, userId] {
      walletsMgr_->setUserId(userId);
      promoteToPrimaryIfNeeded();
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

   setLoginButtonText(loginButtonText_);
}

void BSTerminalMainWindow::onAccountTypeChanged(bs::network::UserType userType, bool enabled)
{
   userType_ = userType;

   if (enabled != accountEnabled_) {
      accountEnabled_ = enabled;
      NotificationCenter::notify(enabled ? bs::ui::NotifyType::AccountEnabled : bs::ui::NotifyType::AccountDisabled, {});
   }

   ui_->widgetChat->setUserType(enabled ? userType : bs::network::UserType::Chat);
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
                  // Primary wallet is one with auth wallet and so we could try to grab chat keys now
                  tryGetChatKeys();
               }
               else if (!walletsMgr_->hdWallets().empty()) {
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
   for (const auto &entry : walletsMgr_->mergeEntries(entries)) {
      const auto &cbTx = [this, entry] (const Tx &tx)
      {
         std::shared_ptr<bs::sync::Wallet> wallet;
         for (const auto &walletId : entry.walletIds) {
            wallet = walletsMgr_->getWalletById(walletId);
            if (wallet) {
               break;
            }
         }
         if (!wallet) {
            return;
         }

         auto txInfo = std::make_shared<TxInfo>();
         txInfo->tx = tx;
         txInfo->txTime = entry.txTime;
         txInfo->value = entry.value;
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

         walletsMgr_->getTransactionDirection(tx, wallet->walletId(), cbDir);
         walletsMgr_->getTransactionMainAddress(tx, wallet->walletId(), (entry.value > 0), cbMainAddr);
      };
      armory_->getTxByHash(entry.txHash, cbTx, true);
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

void BSTerminalMainWindow::onNodeStatus(NodeStatus nodeStatus, bool isSegWitEnabled, RpcStatus rpcStatus)
{
   // Do not use rpcStatus for node status check, it works unreliable for some reasons
   bool isBitcoinCoreOnline = (nodeStatus == NodeStatus_Online);
   if (isBitcoinCoreOnline != isBitcoinCoreOnline_) {
      isBitcoinCoreOnline_ = isBitcoinCoreOnline;
      if (isBitcoinCoreOnline) {
         SPDLOG_LOGGER_INFO(logMgr_->logger(), "ArmoryDB connected to Bitcoin Core RPC");
         NotificationCenter::notify(bs::ui::NotifyType::BitcoinCoreOnline, {});
      } else {
         SPDLOG_LOGGER_ERROR(logMgr_->logger(), "ArmoryDB disconnected from Bitcoin Core RPC");
         NotificationCenter::notify(bs::ui::NotifyType::BitcoinCoreOffline, {});
      }
   }
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
      if (chatClientServicePtr_) {
         chatClientServicePtr_->LogoutFromServer();
      }

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
   auto *button = ui_->pushButtonUser;
   button->setText(text);
   button->setProperty("usernameButton", QVariant(text == loginButtonText_));
   button->setProperty("usernameButtonLoggedIn", QVariant(text != loginButtonText_));
   button->style()->unpolish(button);
   button->style()->polish(button);
   button->update();

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
{
   // do nothing here since we don't know if user will need Private Market before logon to Celer
}

void BSTerminalMainWindow::onCcDefinitionsLoadedFromPub()
{
   promoteToPrimaryIfNeeded();
}

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

void BSTerminalMainWindow::onSyncWallets()
{
   if (walletsMgr_->isSynchronising()) {
      return;
   }

   wasWalletsRegistered_ = false;
   walletsSynched_ = false;
   const auto &progressDelegate = [this](int cur, int total) {
      logMgr_->logger()->debug("Loaded wallet {} of {}", cur, total);
   };

   walletsMgr_->reset();
   walletsMgr_->syncWallets(progressDelegate);
}

void BSTerminalMainWindow::onSignerVisibleChanged()
{
   processDeferredDialogs();
}

void BSTerminalMainWindow::InitWidgets()
{
   authAddrDlg_ = std::make_shared<AuthAddressDialog>(logMgr_->logger(), authManager_
      , assetManager_, applicationSettings_, this);

   InitWalletsView();
   InitPortfolioView();

   ui_->widgetRFQ->initWidgets(mdProvider_, mdCallbacks_, applicationSettings_);

   auto quoteProvider = std::make_shared<QuoteProvider>(assetManager_, logMgr_->logger("message"));
   quoteProvider->ConnectToCelerClient(celerConnection_);

   autoSignQuoteProvider_ = std::make_shared<AutoSignQuoteProvider>(logMgr_->logger(), assetManager_, quoteProvider
      , applicationSettings_, signContainer_, mdCallbacks_, celerConnection_);

   auto dialogManager = std::make_shared<DialogManager>(this);

   ui_->widgetRFQ->init(logMgr_->logger(), celerConnection_, authManager_, quoteProvider, assetManager_
      , dialogManager, signContainer_, armory_, connectionManager_, utxoReservationMgr_, orderListModel_.get());
   ui_->widgetRFQReply->init(logMgr_->logger(), celerConnection_, authManager_, quoteProvider, mdCallbacks_, assetManager_
      , applicationSettings_, dialogManager, signContainer_, armory_, connectionManager_, autoSignQuoteProvider_, utxoReservationMgr_, orderListModel_.get());

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

void BSTerminalMainWindow::networkSettingsReceived(const NetworkSettings &settings, NetworkSettingsClient client)
{
   if (client == NetworkSettingsClient::Login) {
      onLoginProceed(settings);
      return;
   }

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

   networkSettingsReceived_ = true;
   tryInitChatView();
}

void BSTerminalMainWindow::promoteToPrimaryIfNeeded()
{
   // Can't proceed without userId
   if (!walletsMgr_->isUserIdSet()) {
      return;
   }

   auto promoteToPrimary = [this](const std::shared_ptr<bs::sync::hd::Wallet> &wallet) {
      addDeferredDialog([this, wallet] {
         promoteToPrimaryShown_ = true;
         BSMessageBox qry(BSMessageBox::question, tr("Upgrade Wallet"), tr("Enable Trading?")
            , tr("BlockSettle requires you to hold sub-wallets able to interact with our trading system. Do you wish to create them now?"), this);
         if (qry.exec() == QDialog::Accepted) {
            allowAuthAddressDialogShow_ = true;
            walletsMgr_->PromoteHDWallet(wallet->walletId(), [this](bs::error::ErrorCode result) {
               if (result == bs::error::ErrorCode::NoError) {
                  // If wallet was promoted to primary we could try to get chat keys now
                  tryGetChatKeys();
                  walletsMgr_->setUserId(BinaryData::CreateFromHex(celerConnection_->userId()));
               }
            });
         }
      });
   };

   auto primaryWallet = walletsMgr_->getPrimaryWallet();
   if (primaryWallet) {
      for (const auto &leaf : primaryWallet->getLeaves()) {
         if (leaf->type() == bs::core::wallet::Type::ColorCoin) {
            return;
         }
      }
      promoteToPrimary(primaryWallet);
      return;
   }
   for (const auto &hdWallet : walletsMgr_->hdWallets()) {
      if (!hdWallet->isOffline() && !hdWallet->isHardwareWallet()) {
         promoteToPrimary(hdWallet);
         break;
      }
   }
}

void BSTerminalMainWindow::disableCreateTestAccountPrompt()
{
   applicationSettings_->set(ApplicationSettings::HideCreateAccountPromptTestnet, true);
}

void BSTerminalMainWindow::promptToCreateTestAccountIfNeeded()
{
   addDeferredDialog([this] {
      auto envType = static_cast<ApplicationSettings::EnvConfiguration>(applicationSettings_->get(ApplicationSettings::envConfiguration).toInt());
      bool hideCreateAccountTestnet = applicationSettings_->get<bool>(ApplicationSettings::HideCreateAccountPromptTestnet);
      if (envType != ApplicationSettings::EnvConfiguration::Test || hideCreateAccountTestnet) {
         return;
      }
      disableCreateTestAccountPrompt();
      if (bs::network::isTradingEnabled(userType_)) {
         // Do not prompt if user is already logged in
         return;
      }

      CreateAccountPrompt dlg(this);
      int rc = dlg.exec();

      switch (rc) {
         case CreateAccountPrompt::Login:
            onLogin();
            break;
         case CreateAccountPrompt::CreateAccount: {
            auto createTestAccountUrl = applicationSettings_->get<QString>(ApplicationSettings::GetAccount_UrlTest);
            QDesktopServices::openUrl(QUrl(createTestAccountUrl));
            break;
         }
         case CreateAccountPrompt::Cancel:
            break;
      }
   });
}

void BSTerminalMainWindow::showLegacyWarningIfNeeded()
{
   if (applicationSettings_->get<bool>(ApplicationSettings::HideLegacyWalletWarning)) {
      return;
   }
   applicationSettings_->set(ApplicationSettings::HideLegacyWalletWarning, true);
   addDeferredDialog([this] {
      BSMessageBox mbox(BSMessageBox::warning
         , tr("Legacy Wallets")
         , tr("Legacy Address Balances")
         , tr("The BlockSettle Terminal has detected the use of legacy addresses on your hardware wallet.\n\n"
              "The BlockSettle Terminal supports viewing and spending from legacy addresses, but will not support the following actions related to these addresses:\n"
              "- No GUI support for legacy address generation\n"
              "- No trading using legacy address input\n"
              "- No mixing of input types when spending from legacy addresses\n\n"
              "BlockSettle strongly recommends that you move your legacy address balances to native SegWit addresses.")
         , this);
      mbox.exec();
   });
}

void BSTerminalMainWindow::promptSwitchEnv(bool prod)
{
   BSMessageBox mbox(BSMessageBox::question
      , tr("Environment selection")
      , tr("Switch Environment")
      , tr("Do you wish to change to the %1 environment now?").arg(prod ? tr("Production") : tr("Test"))
      , this);
   mbox.setConfirmButtonText(tr("Yes"));
   int rc = mbox.exec();
   if (rc == QDialog::Accepted) {
      if (prod) {
         switchToProdEnv();
      } else {
         switchToTestEnv();
      }
      restartTerminal();
   }
}

void BSTerminalMainWindow::switchToTestEnv()
{
   applicationSettings_->set(ApplicationSettings::envConfiguration
      , static_cast<int>(ApplicationSettings::EnvConfiguration::Test));
   armoryServersProvider_->setupServer(armoryServersProvider_->getIndexOfTestNetServer());
}

void BSTerminalMainWindow::switchToProdEnv()
{
   applicationSettings_->set(ApplicationSettings::envConfiguration
      , static_cast<int>(ApplicationSettings::EnvConfiguration::Production));
   armoryServersProvider_->setupServer(armoryServersProvider_->getIndexOfMainNetServer());
}

void BSTerminalMainWindow::restartTerminal()
{
   lockFile_.unlock();
   QProcess::startDetached(qApp->arguments()[0], qApp->arguments());
   qApp->quit();
}

void BSTerminalMainWindow::processDeferredDialogs()
{
   if(deferredDialogRunning_) {
      return;
   }
   if (signContainer_ && signContainer_->isLocal() && signContainer_->isWindowVisible()) {
      return;
   }

   deferredDialogRunning_ = true;
   while (!deferredDialogs_.empty()) {
      deferredDialogs_.front()(); // run stored lambda
      deferredDialogs_.pop();
   }
   deferredDialogRunning_ = false;
}

void BSTerminalMainWindow::addDeferredDialog(const std::function<void(void)> &deferredDialog)
{
   // multi thread scope, it's safe to call this function from different threads
   QMetaObject::invokeMethod(this, [this, deferredDialog] {
      // single thread scope (main thread), it's safe to push to deferredDialogs_
      // and check deferredDialogRunning_ variable
      deferredDialogs_.push(deferredDialog);
      processDeferredDialogs();
   }, Qt::QueuedConnection);
}
