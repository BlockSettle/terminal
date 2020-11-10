/*

***********************************************************************************
* Copyright (C) 2018 - 2020, BlockSettle AB
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
#include <streambuf>
#include <thread>

#include "ArmoryServersProvider.h"
#include "AssetManager.h"
#include "AuthAddressDialog.h"
#include "AuthAddressManager.h"
#include "AutheIDClient.h"
#include "AutoSignQuoteProvider.h"
#include "Bip15xDataConnection.h"
#include "BootstrapDataManager.h"
#include "BSMarketDataProvider.h"
#include "BSMessageBox.h"
#include "BSTerminalSplashScreen.h"
#include "CCFileManager.h"
#include "CCPortfolioModel.h"
#include "CCTokenEntryDialog.h"
#include "CelerAccountInfoDialog.h"
#include "ColoredCoinServer.h"
#include "ConnectionManager.h"
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
#include "MarketDataProvider.h"
#include "MDCallbacksQt.h"
#include "NewAddressDialog.h"
#include "NewWalletDialog.h"
#include "NotificationCenter.h"
#include "OpenURIDialog.h"
#include "OrderListModel.h"
#include "PubKeyLoader.h"
#include "QuoteProvider.h"
#include "RequestReplyCommand.h"
#include "RetryingDataConnection.h"
#include "SelectWalletDialog.h"
#include "Settings/ConfigDialog.h"
#include "SignersProvider.h"
#include "SslCaBundle.h"
#include "SslDataConnection.h"
#include "StatusBarView.h"
#include "StringUtils.h"
#include "SystemFileUtils.h"
#include "TabWithShortcut.h"
#include "TransactionsViewModel.h"
#include "TransactionsWidget.h"
#include "TransportBIP15x.h"
#include "UiUtils.h"
#include "UserScriptRunner.h"
#include "UtxoReservationManager.h"
#include "Wallets/SyncHDWallet.h"
#include "Wallets/SyncWalletsManager.h"
#include "WsDataConnection.h"

#include "ui_BSTerminalMainWindow.h"

namespace {
   const auto kAutoLoginTimer = std::chrono::seconds(10);
}

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
   setupInfoWidget();

   loginButtonText_ = tr("Login");

   logMgr_ = std::make_shared<bs::LogManager>();
   logMgr_->add(applicationSettings_->GetLogsConfig());
   logMgr_->logger()->debug("Settings loaded from {}", applicationSettings_->GetSettingsPath().toStdString());

   bool licenseAccepted = showStartupDialog();
   if (!licenseAccepted) {
      QMetaObject::invokeMethod(this, []() {
         qApp->exit(EXIT_FAILURE);
      }, Qt::QueuedConnection);
      return;
   }

   initBootstrapDataManager();

   nextArmoryReconnectAttempt_ = std::chrono::steady_clock::now();
   signersProvider_= std::make_shared<SignersProvider>(applicationSettings_);
   armoryServersProvider_ = std::make_shared<ArmoryServersProvider>(applicationSettings_, bootstrapDataManager_);

   if (applicationSettings_->get<QString>(ApplicationSettings::armoryDbName).isEmpty()) {
      const auto env = static_cast<ApplicationSettings::EnvConfiguration>(applicationSettings_->get<int>(ApplicationSettings::envConfiguration));
      switch(env) {
      case ApplicationSettings::EnvConfiguration::Production:
         armoryServersProvider_->setupServer(armoryServersProvider_->getIndexOfMainNetServer(), false);
         break;
      case ApplicationSettings::EnvConfiguration::Test:
#ifndef PRODUCTION_BUILD
      case ApplicationSettings::EnvConfiguration::Staging:
#endif
         armoryServersProvider_->setupServer(armoryServersProvider_->getIndexOfTestNetServer(), false);
         break;
      }
   }

   splashScreen.show();

   connect(ui_->actionQuit, &QAction::triggered, qApp, &QCoreApplication::quit);

   bs::UtxoReservation::init(logMgr_->logger());

   setupIcon();
   UiUtils::setupIconFont(this);
   NotificationCenter::createInstance(logMgr_->logger(), applicationSettings_, ui_.get(), sysTrayIcon_, this);

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

   cbApproveChat_ = PubKeyLoader::getApprovingCallback(PubKeyLoader::KeyType::Chat
      , this, applicationSettings_, bootstrapDataManager_);
   cbApproveProxy_ = PubKeyLoader::getApprovingCallback(PubKeyLoader::KeyType::Proxy
      , this, applicationSettings_, bootstrapDataManager_);
   cbApproveCcServer_ = PubKeyLoader::getApprovingCallback(PubKeyLoader::KeyType::CcServer
      , this, applicationSettings_, bootstrapDataManager_);
   cbApproveExtConn_ = PubKeyLoader::getApprovingCallback(PubKeyLoader::KeyType::ExtConnector
      , this, applicationSettings_, bootstrapDataManager_);

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

   UpdateMainWindowAppearence();
   setWidgetsAuthorized(false);

   updateControlEnabledState();

   InitWidgets();

   loginApiKeyEncrypted_ = applicationSettings_->get<std::string>(ApplicationSettings::LoginApiKey);

#ifdef PRODUCTION_BUILD
   const bool showEnvSelector = false;
#else
   const bool showEnvSelector = true;
#endif
   ui_->prodEnvSettings->setVisible(showEnvSelector);
   ui_->testEnvSettings->setVisible(showEnvSelector);
}

void BSTerminalMainWindow::onBsConnectionDisconnected()
{
   onCelerDisconnected();
}

void BSTerminalMainWindow::onBsConnectionFailed()
{
   SPDLOG_LOGGER_ERROR(logMgr_->logger(), "BsClient disconnected unexpectedly");
   showError(tr("Network error"), tr("Connection to BlockSettle server failed"));
}

void BSTerminalMainWindow::onInitWalletDialogWasShown()
{
   initialWalletCreateDialogShown_ = true;
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
      const float coeff = (float)0.9999;   // some coefficient that prevents oversizing of main window on HiRes display on Windows
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
   action_send_ = new QAction(tr("Send Bitcoin"), this);
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

#if defined(Q_OS_WIN)
   ui_->tabWidget->setProperty("onWindows", QVariant(true));
#elif defined(Q_OS_LINUX)
   ui_->tabWidget->setProperty("onLinux", QVariant(true));
#else
   ui_->tabWidget->setProperty("onMacos", QVariant(true));
#endif

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

void BSTerminalMainWindow::setupInfoWidget()
{
   const bool show = applicationSettings_->get<bool>(ApplicationSettings::ShowInfoWidget);
   ui_->infoWidget->setVisible(show);

   if (!show) {
      return;
   }

   connect(ui_->introductionBtn, &QPushButton::clicked, this, []() {
      QDesktopServices::openUrl(QUrl(QLatin1String("https://www.youtube.com/watch?v=mUqKq9GKjmI")));
   });
   connect(ui_->tutorialsButton, &QPushButton::clicked, this, []() {
      QDesktopServices::openUrl(QUrl(QLatin1String("https://blocksettle.com/tutorials")));
   });
   connect(ui_->closeBtn, &QPushButton::clicked, this, [this]() {
      ui_->infoWidget->setVisible(false);
      applicationSettings_->set(ApplicationSettings::ShowInfoWidget, false);
   });
}

void BSTerminalMainWindow::initConnections()
{
   connectionManager_ = std::make_shared<ConnectionManager>(logMgr_->logger("message"));
   connectionManager_->setCaBundle(bs::caBundlePtr(), bs::caBundleSize());

   celerConnection_ = std::make_shared<CelerClientProxy>(logMgr_->logger());
   connect(celerConnection_.get(), &CelerClientQt::OnConnectedToServer, this, &BSTerminalMainWindow::onCelerConnected);
   connect(celerConnection_.get(), &CelerClientQt::OnConnectionClosed, this, &BSTerminalMainWindow::onCelerDisconnected);
   connect(celerConnection_.get(), &CelerClientQt::OnConnectionError, this, &BSTerminalMainWindow::onCelerConnectionError, Qt::QueuedConnection);

   mdCallbacks_ = std::make_shared<MDCallbacksQt>();
   mdProvider_ = std::make_shared<BSMarketDataProvider>(connectionManager_
      , logMgr_->logger("message"), mdCallbacks_.get(), true, false);
   connect(mdCallbacks_.get(), &MDCallbacksQt::UserWantToConnectToMD, this, &BSTerminalMainWindow::acceptMDAgreement);
   connect(mdCallbacks_.get(), &MDCallbacksQt::WaitingForConnectionDetails, this, [this] {
      auto env = static_cast<ApplicationSettings::EnvConfiguration>(
               applicationSettings_->get<int>(ApplicationSettings::envConfiguration));
      mdProvider_->SetConnectionSettings(PubKeyLoader::serverHostName(PubKeyLoader::KeyType::MdServer, env)
         , PubKeyLoader::serverHttpsPort());
   });
}

void BSTerminalMainWindow::LoadWallets()
{
   logMgr_->logger()->debug("Loading wallets");

   connect(walletsMgr_.get(), &bs::sync::WalletsManager::walletsReady, this, [this] {
      ui_->widgetRFQ->setWalletsManager(walletsMgr_);
      ui_->widgetRFQReply->setWalletsManager(walletsMgr_);
      autoSignQuoteProvider_->setWalletsManager(walletsMgr_);
      autoSignRFQProvider_->setWalletsManager(walletsMgr_);
   });
   connect(walletsMgr_.get(), &bs::sync::WalletsManager::walletsSynchronized, this, [this] {
      walletsSynched_ = true;
      updateControlEnabledState();
      CompleteDBConnection();
      act_->onRefresh({}, true);
      tryGetChatKeys();
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

   connect(walletsMgr_.get(), &bs::sync::WalletsManager::AuthLeafCreated, this, &BSTerminalMainWindow::onAuthLeafCreated);

   onSyncWallets();
}

void BSTerminalMainWindow::InitAuthManager()
{
   authManager_ = std::make_shared<AuthAddressManager>(logMgr_->logger(), armory_);
   authManager_->init(applicationSettings_, walletsMgr_, signContainer_);

   connect(authManager_.get(), &AuthAddressManager::AddrVerifiedOrRevoked, this, [](const QString &addr, int state) {
      NotificationCenter::notify(bs::ui::NotifyType::AuthAddress, { addr, state });
   });
   connect(authManager_.get(), &AuthAddressManager::AuthWalletCreated, this, [this](const QString &walletId) {
      if (authAddrDlg_ && walletId.isEmpty()) {
         openAuthManagerDialog();
      }
   });

   authManager_->SetLoadedValidationAddressList(bootstrapDataManager_->GetAuthValidationList());
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
      , resultHost, resultPort, netType, connectionManager_, nullptr
      , SignContainer::OpMode::Remote, false
      , signersProvider_->remoteSignerKeysDir(), signersProvider_->remoteSignerKeysFile(), ourNewKeyCB);

   bs::network::BIP15xPeers peers;
   for (const auto &signer : signersProvider_->signers()) {
      try {
         const BinaryData signerKey = BinaryData::CreateFromHex(signer.key.toStdString());
         peers.push_back(bs::network::BIP15xPeer(signer.serverId(), signerKey));
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
   QString localSignerPort;
   NetworkType netType = applicationSettings_->get<NetworkType>(ApplicationSettings::netType);

   for (int attempts = 0; attempts < 10; ++attempts) {
      // https://tools.ietf.org/html/rfc6335
      // the Dynamic Ports, also known as the Private or Ephemeral Ports,
      // from 49152-65535 (never assigned)
      auto port = 49152 + rand() % 16000;

      auto portToTest = QString::number(port);

      if (!SignerConnectionExists(localSignerHost, portToTest)) {
         localSignerPort = portToTest;
         break;
      } else {
         logMgr_->logger()->error("[BSTerminalMainWindow::createLocalSigner] attempt {} : port {} used"
                        , port);
      }
   }

   if (localSignerPort.isEmpty()) {
      logMgr_->logger()->error("[BSTerminalMainWindow::createLocalSigner] failed to find not busy port");
      return nullptr;
   }

   const bool startLocalSignerProcess = true;
   return std::make_shared<LocalSigner>(logMgr_->logger()
      , applicationSettings_->GetHomeDir(), netType
      , localSignerPort, connectionManager_, nullptr
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
         ui_->widgetWallets->onNewWallet();
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
      const bool txCreationEnabled = !walletsMgr_->hdWallets().empty()
         && armory_->isOnline() && signContainer_ && signContainer_->isReady();

      action_send_->setEnabled(txCreationEnabled);
      ui_->actionOpenURI->setEnabled(txCreationEnabled);
   }
   // Do not allow login until wallets synced (we need to check if user has primary wallet or not).
   // Should be OK for both local and remote signer.
   bool loginAllowed = walletsSynched_ && loginApiKeyEncrypted().empty();
   ui_->pushButtonUser->setEnabled(loginAllowed);
   action_login_->setEnabled(true);

   action_login_->setVisible(!celerConnection_->IsConnected());
   action_login_->setEnabled(loginAllowed);
   action_logout_->setVisible(celerConnection_->IsConnected());
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
   startupDialog.applySelectedConnectivity();

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

   connect(mdCallbacks_.get(), &MDCallbacksQt::MDUpdate, assetManager_.get(), &AssetManager::onMDUpdate);

   ccFileManager_->SetLoadedDefinitions(bootstrapDataManager_->GetCCDefinitions());
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
   if (chatInitState_ != ChatInitState::NoStarted || !gotChatKeys_) {
      return;
   }
   chatInitState_ = ChatInitState::InProgress;

   chatClientServicePtr_ = std::make_shared<Chat::ChatClientService>();

   connect(chatClientServicePtr_.get(), &Chat::ChatClientService::initDone, this, [this]() {
      const bool isProd = applicationSettings_->get<int>(ApplicationSettings::envConfiguration) ==
         static_cast<int>(ApplicationSettings::EnvConfiguration::Production);
      const auto env = isProd ? bs::network::otc::Env::Prod : bs::network::otc::Env::Test;

      ui_->widgetChat->init(connectionManager_, env, chatClientServicePtr_,
         logMgr_->logger("chat"), walletsMgr_, authManager_, armory_, signContainer_,
         mdCallbacks_, assetManager_, utxoReservationMgr_, applicationSettings_);

      connect(chatClientServicePtr_->getClientPartyModelPtr().get(), &Chat::ClientPartyModel::userPublicKeyChanged,
         this, [this](const Chat::UserPublicKeyInfoList& userPublicKeyInfoList) {
         addDeferredDialog([this, userPublicKeyList = userPublicKeyInfoList]() {
            ui_->widgetChat->onUserPublicKeyChanged(userPublicKeyList);
         });
      }, Qt::QueuedConnection);

      chatInitState_ = ChatInitState::Done;
      tryLoginIntoChat();
   });

   auto env = static_cast<ApplicationSettings::EnvConfiguration>(
            applicationSettings_->get<int>(ApplicationSettings::envConfiguration));

   Chat::ChatSettings chatSettings;
   chatSettings.connectionManager = connectionManager_;
   chatSettings.chatPrivKey = chatPrivKey_;
   chatSettings.chatPubKey = chatPubKey_;
   chatSettings.chatServerHost = PubKeyLoader::serverHostName(PubKeyLoader::KeyType::Chat, env);
   chatSettings.chatServerPort = PubKeyLoader::serverHttpPort();
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
      // Reset API key if it was stored (as it won't be possible to decrypt it)
      applicationSettings_->reset(ApplicationSettings::LoginApiKey);
      loginApiKeyEncrypted_.clear();
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
      initApiKeyLogins();
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
   ui_->widgetTransactions->init(walletsMgr_, armory_, utxoReservationMgr_, signContainer_, applicationSettings_
                                , logMgr_->logger("ui"));
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
         parent_->armoryReconnectDelay_ = 0;
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
         parent_->armoryRestartCount_ = 0;
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

   auto now = std::chrono::steady_clock::now();
   std::chrono::milliseconds nextConnDelay(0);
   if (now < nextArmoryReconnectAttempt_) {
      nextConnDelay = std::chrono::duration_cast<std::chrono::milliseconds>(
         nextArmoryReconnectAttempt_ - now);
   }
   if (nextConnDelay != std::chrono::milliseconds::zero()) {

      auto delaySec = std::chrono::duration_cast<std::chrono::seconds>(nextConnDelay);
      SPDLOG_LOGGER_DEBUG(logMgr_->logger("ui")
         , "restart armory connection in {} second", nextConnDelay.count());

      QTimer::singleShot(nextConnDelay, this, &BSTerminalMainWindow::ArmoryIsOffline);
      return;
   }

   if (walletsMgr_) {
      walletsMgr_->unregisterWallets();
   }
   updateControlEnabledState();

   //increase reconnect delay
   armoryReconnectDelay_ = armoryReconnectDelay_ % 2 ?
      armoryReconnectDelay_ * 2 : armoryReconnectDelay_ + 1;
   armoryReconnectDelay_ = std::max(armoryReconnectDelay_, unsigned(60));
   nextArmoryReconnectAttempt_ =
      std::chrono::steady_clock::now() + std::chrono::seconds(armoryReconnectDelay_);

   connectArmory();

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

void BSTerminalMainWindow::initBootstrapDataManager()
{
   bootstrapDataManager_ = std::make_shared<BootstrapDataManager>(logMgr_->logger(), applicationSettings_);
   if (bootstrapDataManager_->hasLocalFile()) {
      bootstrapDataManager_->loadFromLocalFile();
   } else {
      // load from resources
      const QString filePathInResources = applicationSettings_->bootstrapResourceFileName();

      QFile file;
      file.setFileName(filePathInResources);
      if (file.open(QIODevice::ReadOnly)) {
         const std::string bootstrapData = file.readAll().toStdString();

         bootstrapDataManager_->setReceivedData(bootstrapData);
      } else {
         logMgr_->logger()->error("[BSTerminalMainWindow::initBootstrapDataManager] failed to locate bootstrap file in resources: {}"
                        , filePathInResources.toStdString());
      }

   }
}

void BSTerminalMainWindow::MainWinACT::onTxBroadcastError(const std::string& requestId, const BinaryData &txHash, int errCode
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

void BSTerminalMainWindow::MainWinACT::onZCReceived(const std::string& requestId, const std::vector<bs::TXEntry>& zcs)
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
      auto env = static_cast<ApplicationSettings::EnvConfiguration>(
               applicationSettings_->get<int>(ApplicationSettings::envConfiguration));
      auto trackerHostName = PubKeyLoader::serverHostName(PubKeyLoader::KeyType::CcServer, env);
      trackerClient_->openConnection(trackerHostName, PubKeyLoader::serverHttpPort(), cbApproveCcServer_);
   }
}

void BSTerminalMainWindow::connectSigner()
{
   if (!signContainer_) {
      return;
   }

   signContainer_->Start();
}

bool BSTerminalMainWindow::createPrimaryWallet()
{
   auto primaryWallet = walletsMgr_->getPrimaryWallet();
   if (primaryWallet) {
      return true;
   }


   for (const auto &wallet : walletsMgr_->hdWallets()) {
      if (!wallet->isOffline() && !wallet->isHardwareWallet()) {
         BSMessageBox qry(BSMessageBox::question, tr("Promote to primary wallet"), tr("Promote to primary wallet?")
            , tr("To trade through BlockSettle, you are required to have a wallet which"
               " supports the sub-wallets required to interact with the system. Each Terminal"
               " may only have one Primary Wallet. Do you wish to promote '%1'?")
            .arg(QString::fromStdString(wallet->name())), this);
         if (qry.exec() == QDialog::Accepted) {
            walletsMgr_->PromoteWalletToPrimary(wallet->walletId());
            return true;
         }
      }
   }

   CreatePrimaryWalletPrompt dlg;
   int rc = dlg.exec();
   if (rc == CreatePrimaryWalletPrompt::CreateWallet) {
      ui_->widgetWallets->CreateNewWallet();
   } else if (rc == CreatePrimaryWalletPrompt::ImportWallet) {
      ui_->widgetWallets->ImportNewWallet();
   }

   return true;
}

void BSTerminalMainWindow::onCreatePrimaryWalletRequest()
{
   bool result = createPrimaryWallet();

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
      createPrimaryWallet();
      return;
   }

   const auto defWallet = walletsMgr_->getDefaultWallet();
   std::string selWalletId = defWallet ? defWallet->walletId() : std::string{};

   if (ui_->tabWidget->currentWidget() == ui_->widgetWallets) {
      auto wallets = ui_->widgetWallets->getSelectedWallets();
      if (!wallets.empty()) {
         selWalletId = wallets[0].ids[0];
      } else {
         wallets = ui_->widgetWallets->getFirstWallets();

         if (!wallets.empty()) {
            selWalletId = wallets[0].ids[0];
         }
      }
   }
   SelectWalletDialog selectWalletDialog(walletsMgr_, selWalletId, this);
   selectWalletDialog.exec();

   if (selectWalletDialog.result() == QDialog::Rejected) {
      return;
   }

   const auto &wallet = walletsMgr_->getWalletById(selectWalletDialog.getSelectedWallet());
   NewAddressDialog newAddressDialog(wallet, this);
   newAddressDialog.exec();
}

void BSTerminalMainWindow::onSend()
{
   std::string selectedWalletId;

   if (ui_->tabWidget->currentWidget() == ui_->widgetWallets) {
      auto wallet = ui_->widgetWallets->getSelectedHdWallet();
      if (wallet.ids.empty()) {
         const auto &priWallet = walletsMgr_->getPrimaryWallet();
         if (priWallet) {
            wallet.ids.push_back(priWallet->walletId());
         }
      }
      if (!wallet.ids.empty()) {
         selectedWalletId = wallet.ids[0];
      }
   } else {
      selectedWalletId = applicationSettings_->getDefaultWalletId();
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
      dlg->SelectWallet(selectedWalletId, UiUtils::WalletsTypes::None);
   }

   DisplayCreateTransactionDialog(dlg);
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
   auto supportDlgCb = [supportDlg] (int tab, QString title) {
      return [supportDlg, tab, title]() {
         supportDlg->setTab(tab);
         supportDlg->setWindowTitle(title);
         supportDlg->show();
      };
   };

   connect(ui_->actionCreateNewWallet, &QAction::triggered, this, [ww = ui_->widgetWallets]{ ww->onNewWallet(); });
   connect(ui_->actionOpenURI, &QAction::triggered, this, [this]{ openURIDialog(); });
   connect(ui_->actionAuthenticationAddresses, &QAction::triggered, this, &BSTerminalMainWindow::openAuthManagerDialog);
   connect(ui_->actionSettings, &QAction::triggered, this, [=]() { openConfigDialog(); });
   connect(ui_->actionAccountInformation, &QAction::triggered, this, &BSTerminalMainWindow::openAccountInfoDialog);
   connect(ui_->actionEnterColorCoinToken, &QAction::triggered, this, &BSTerminalMainWindow::openCCTokenDialog);
   connect(ui_->actionAbout, &QAction::triggered, aboutDlgCb(0));
   connect(ui_->actionVersion, &QAction::triggered, aboutDlgCb(3));
   connect(ui_->actionGuides, &QAction::triggered, supportDlgCb(0, QObject::tr("Guides")));
   connect(ui_->actionVideoTutorials, &QAction::triggered, supportDlgCb(1, QObject::tr("Video Tutorials")));
   connect(ui_->actionContact, &QAction::triggered, supportDlgCb(2, QObject::tr("Support")));

   onUserLoggedOut();

#ifndef Q_OS_MAC
   ui_->horizontalFrame->hide();
   ui_->menubar->setCornerWidget(ui_->loginGroupWidget);
#endif

#ifndef PRODUCTION_BUILD
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
#else
   ui_->prodEnvSettings->setVisible(false);
   ui_->testEnvSettings->setVisible(false);
#endif // !PRODUCTION_BUILD
}

void BSTerminalMainWindow::openAuthManagerDialog()
{
   authAddrDlg_->exec();
}

void BSTerminalMainWindow::openConfigDialog(bool showInNetworkPage)
{
   auto oldEnv = static_cast<ApplicationSettings::EnvConfiguration>(
            applicationSettings_->get<int>(ApplicationSettings::envConfiguration));

   ConfigDialog configDialog(applicationSettings_, armoryServersProvider_, signersProvider_, signContainer_, walletsMgr_, this);
   connect(&configDialog, &ConfigDialog::reconnectArmory, this, &BSTerminalMainWindow::onArmoryNeedsReconnect);

   if (showInNetworkPage) {
      configDialog.popupNetworkSettings();
   }

   int rc = configDialog.exec();

   UpdateMainWindowAppearence();

   auto newEnv = static_cast<ApplicationSettings::EnvConfiguration>(
            applicationSettings_->get<int>(ApplicationSettings::envConfiguration));
   if (rc == QDialog::Accepted && newEnv != oldEnv) {
      bool prod = newEnv == ApplicationSettings::EnvConfiguration::Production;
      BSMessageBox mbox(BSMessageBox::question
         , tr("Environment selection")
         , tr("Switch Environment")
         , tr("Do you wish to change to the %1 environment now?").arg(prod ? tr("Production") : tr("Test"))
         , this);
      mbox.setConfirmButtonText(tr("Yes"));
      int rc = mbox.exec();
      if (rc == QDialog::Accepted) {
         restartTerminal();
      }
   }
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
}

void BSTerminalMainWindow::onLogin()
{
   if (!action_login_->isEnabled()) {
      return;
   }
   auto envType = static_cast<ApplicationSettings::EnvConfiguration>(applicationSettings_->get(ApplicationSettings::envConfiguration).toInt());

   if (walletsSynched_ && !walletsMgr_->getPrimaryWallet()) {
      addDeferredDialog([this] {
         createPrimaryWallet();
      });
      return;
   }

   auto bsClient = createClient();

   auto logger = logMgr_->logger("proxy");
   LoginWindow loginDialog(logger, bsClient, applicationSettings_, this);

   int rc = loginDialog.exec();
   if (rc != QDialog::Accepted && !loginDialog.result()) {
      return;
   }

   bool isRegistered = (loginDialog.result()->userType == bs::network::UserType::Market
      || loginDialog.result()->userType == bs::network::UserType::Trading
      || loginDialog.result()->userType == bs::network::UserType::Dealing);

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
      dlg.enableRichText();
      dlg.exec();
      return;
   }

   if (!isRegistered && envType == ApplicationSettings::EnvConfiguration::Production) {
      auto createAccountUrl = applicationSettings_->get<QString>(ApplicationSettings::GetAccount_UrlProd);
      BSMessageBox dlg(BSMessageBox::info, tr("Create Account")
         , tr("Create a BlockSettle account")
         , tr("<p>Login requires an account - create one in minutes on blocksettle.com</p>"
              "<p>Once you have registered, return to login in the Terminal.</p>"
              "<a href=\"%1\"><span style=\"text-decoration: underline;color:%2;\">Create Account Now</span></a>")
         .arg(createAccountUrl).arg(BSMessageBox::kUrlColor), this);
      dlg.setOkVisible(false);
      dlg.setCancelVisible(true);
      dlg.enableRichText();
      dlg.exec();
      return;
   }

   activateClient(bsClient, *loginDialog.result(), loginDialog.email().toStdString());
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
   ui_->widgetRFQ->onUserConnected(userType_);
   ui_->widgetRFQReply->onUserConnected(userType_);

   const auto userId = BinaryData::CreateFromHex(celerConnection_->userId());
   const auto &deferredDialog = [this, userId] {
      walletsMgr_->setUserId(userId);
      enableTradingIfNeeded();
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

   if (enabled != accountEnabled_ && userType != bs::network::UserType::Chat) {
      accountEnabled_ = enabled;
      NotificationCenter::notify(enabled ? bs::ui::NotifyType::AccountEnabled : bs::ui::NotifyType::AccountDisabled, {});
   }

   authManager_->setUserType(userType);

   ui_->widgetChat->setUserType(enabled ? userType : bs::network::UserType::Chat);
}

void BSTerminalMainWindow::onCelerConnected()
{
   onUserLoggedIn();
   updateControlEnabledState();
}

void BSTerminalMainWindow::onCelerDisconnected()
{
   onUserLoggedOut();
   celerConnection_->CloseConnection();
   updateControlEnabledState();
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
   lines << (txInfo.tx.isRBF() ? tr("RBF Enabled") : tr("RBF Disabled"));
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
         SPDLOG_LOGGER_INFO(logMgr_->logger(), "BlockSettleDB connected to Bitcoin Core RPC");
         NotificationCenter::notify(bs::ui::NotifyType::BitcoinCoreOnline, {});
      } else {
         SPDLOG_LOGGER_ERROR(logMgr_->logger(), "BlockSettleDB disconnected from Bitcoin Core RPC");
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

   auto explorerTabShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+6")), this);
   explorerTabShortcut->setContext(Qt::WindowShortcut);
   connect(explorerTabShortcut, &QShortcut::activated, [this](){ ui_->tabWidget->setCurrentIndex(5);});

   auto chartsTabShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+7")), this);
   chartsTabShortcut->setContext(Qt::WindowShortcut);
   connect(chartsTabShortcut, &QShortcut::activated, [this](){ ui_->tabWidget->setCurrentIndex(6);});

   auto chatTabShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+8")), this);
   chatTabShortcut->setContext(Qt::WindowShortcut);
   connect(chatTabShortcut, &QShortcut::activated, [this](){ ui_->tabWidget->setCurrentIndex(7);});

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
               , tr("Import BlockSettleDB ID Key?")
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
               , tr("Import BlockSettleDB ID Key?")
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
   updateControlEnabledState();
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

   auto quoteProvider = std::make_shared<QuoteProvider>(assetManager_
      , logMgr_->logger("message"));
   quoteProvider->ConnectToCelerClient(celerConnection_);

   const auto &logger = logMgr_->logger();
   const auto aqScriptRunner = new AQScriptRunner(quoteProvider, signContainer_
      , mdCallbacks_, assetManager_, logger);
   if (!applicationSettings_->get<std::string>(ApplicationSettings::ExtConnName).empty()
      && !applicationSettings_->get<std::string>(ApplicationSettings::ExtConnHost).empty()
      && !applicationSettings_->get<std::string>(ApplicationSettings::ExtConnPort).empty()
      && !applicationSettings_->get<std::string>(ApplicationSettings::ExtConnPubKey).empty()) {
      ExtConnections extConns;
      logger->debug("Setting up ext connection");

      const auto &clientKeyPath = SystemFilePaths::appDataLocation() + "/extConnKey";
      bs::network::ws::PrivateKey privKeyClient;
      std::ifstream privKeyReader(clientKeyPath, std::ios::binary);
      if (privKeyReader.is_open()) {
         std::string str;
         str.assign(std::istreambuf_iterator<char>(privKeyReader)
            , std::istreambuf_iterator<char>());
         privKeyClient.reserve(str.size());
         std::for_each(str.cbegin(), str.cend(), [&privKeyClient](char c) {
            privKeyClient.push_back(c);
         });
      }
      if (privKeyClient.empty()) {
         logger->debug("Creating new ext connection key");
         privKeyClient = bs::network::ws::generatePrivKey();
         std::ofstream privKeyWriter(clientKeyPath, std::ios::out|std::ios::binary);
         privKeyWriter.write((char *)&privKeyClient[0], privKeyClient.size());
         const auto &pubKeyClient = bs::network::ws::publicKey(privKeyClient);
         applicationSettings_->set(ApplicationSettings::ExtConnOwnPubKey
            , QString::fromStdString(bs::toHex(pubKeyClient)));
      }
      const auto &certClient = bs::network::ws::generateSelfSignedCert(privKeyClient);
      const auto &srvPubKey = applicationSettings_->get<std::string>(ApplicationSettings::ExtConnPubKey);
      SslDataConnectionParams clientParams;
      clientParams.useSsl = true;
      clientParams.cert = certClient;
      clientParams.privKey = privKeyClient;
      clientParams.allowSelfSigned = true;
      clientParams.skipHostNameChecks = true;
      clientParams.verifyCallback = [srvPubKey, this](const std::string &pubKey) -> bool {
         if (BinaryData::CreateFromHex(srvPubKey).toBinStr() == pubKey) {
            return true;
         }
         QMetaObject::invokeMethod(this, [pubKey] {
            BSMessageBox(BSMessageBox::warning, tr("External Connection error")
               , tr("Invalid server key: %1").arg(QString::fromStdString(bs::toHex(pubKey)))).exec();
         });
         return false;
      };

      RetryingDataConnectionParams retryingParams;
      retryingParams.connection = std::make_unique<SslDataConnection>(logger, clientParams);
      auto connection = std::make_shared<RetryingDataConnection>(logger, std::move(retryingParams));

      if (connection->openConnection(applicationSettings_->get<std::string>(ApplicationSettings::ExtConnHost)
         , applicationSettings_->get<std::string>(ApplicationSettings::ExtConnPort)
         , aqScriptRunner->getExtConnListener().get())) {
         extConns[applicationSettings_->get<std::string>(ApplicationSettings::ExtConnName)] = connection;
      }
      aqScriptRunner->setExtConnections(extConns);
   }

   autoSignQuoteProvider_ = std::make_shared<AutoSignAQProvider>(logger
      , aqScriptRunner, applicationSettings_, signContainer_, celerConnection_);

   const auto rfqScriptRunner = new RFQScriptRunner(mdCallbacks_, logger, nullptr);
   autoSignRFQProvider_ = std::make_shared<AutoSignRFQProvider>(logger
      , rfqScriptRunner, applicationSettings_, signContainer_, celerConnection_);

   auto dialogManager = std::make_shared<DialogManager>(this);

   ui_->widgetRFQ->init(logger, celerConnection_, authManager_, quoteProvider
      , assetManager_, dialogManager, signContainer_, armory_, autoSignRFQProvider_
      , utxoReservationMgr_, orderListModel_.get());
   ui_->widgetRFQReply->init(logger, celerConnection_, authManager_
      , quoteProvider, mdCallbacks_, assetManager_, applicationSettings_, dialogManager
      , signContainer_, armory_, connectionManager_, autoSignQuoteProvider_
      , utxoReservationMgr_, orderListModel_.get());

   connect(ui_->widgetRFQ, &RFQRequestWidget::requestPrimaryWalletCreation, this
      , &BSTerminalMainWindow::onCreatePrimaryWalletRequest);
   connect(ui_->widgetRFQReply, &RFQReplyWidget::requestPrimaryWalletCreation, this
      , &BSTerminalMainWindow::onCreatePrimaryWalletRequest);
   connect(ui_->widgetRFQ, &RFQRequestWidget::loginRequested, this
      , &BSTerminalMainWindow::onLogin);

   connect(ui_->tabWidget, &QTabWidget::tabBarClicked, this,
      [requestRFQ = QPointer<RFQRequestWidget>(ui_->widgetRFQ)
         , replyRFQ = QPointer<RFQReplyWidget>(ui_->widgetRFQReply)
         , tabWidget = QPointer<QTabWidget>(ui_->tabWidget)] (int index)
   {
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

void BSTerminalMainWindow::enableTradingIfNeeded()
{
   // Can't proceed without userId
   if (!walletsMgr_->isUserIdSet()) {
      return;
   }

   auto enableTradingFunc = [this](const std::shared_ptr<bs::sync::hd::Wallet> &wallet) {
      addDeferredDialog([this, wallet] {
         BSMessageBox qry(BSMessageBox::question, tr("Upgrade Wallet"), tr("Enable Trading")
            , tr("BlockSettle requires you to hold sub-wallets with Authentication Addresses to interact with our trading system.<br><br>"
                  "You will be able to trade up to %1 bitcoin per trade once your Authentication Address has been submitted.<br><br>"
                  "After %2 trades your Authentication Address will be verified and your trading limit removed.<br><br>"
                  "Do you wish to enable XBT trading?").arg(bs::XBTAmount(tradeSettings_->xbtTier1Limit).GetValueBitcoin()).arg(tradeSettings_->authRequiredSettledTrades)
            , this);
         qry.enableRichText();
         if (qry.exec() == QDialog::Accepted) {
            walletsMgr_->EnableXBTTradingInWallet(wallet->walletId(), [this](bs::error::ErrorCode result) {
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
      if (!primaryWallet->tradingEnabled()) {
         enableTradingFunc(primaryWallet);
      }
   }
}

void BSTerminalMainWindow::showLegacyWarningIfNeeded()
{
   if (applicationSettings_->get<bool>(ApplicationSettings::HideLegacyWalletWarning)) {
      return;
   }
   applicationSettings_->set(ApplicationSettings::HideLegacyWalletWarning, true);
   addDeferredDialog([this] {
      int forcedWidth = 640;
      BSMessageBox mbox(BSMessageBox::info
         , tr("Legacy Wallets")
         , tr("Legacy Address Balances")
         , tr("The BlockSettle Terminal has detected the use of legacy addresses in your wallet.\n\n"
              "The BlockSettle Terminal supports viewing and spending from legacy addresses, but will not support the following actions related to these addresses:\n\n"
              "- GUI support for legacy address generation\n"
              "- Trading and settlement using legacy inputs\n\n"
              "BlockSettle strongly recommends that you move your legacy address balances to native SegWit addresses.")
         , {}
         , forcedWidth
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

std::shared_ptr<BsClientQt> BSTerminalMainWindow::createClient()
{
   auto logger = logMgr_->logger("proxy");
   auto bsClient = std::make_shared<BsClientQt>(logger);

   bs::network::BIP15xParams params;
   params.ephemeralPeers = true;
   params.authMode = bs::network::BIP15xAuthMode::OneWay;
   const auto &bip15xTransport = std::make_shared<bs::network::TransportBIP15xClient>(logger, params);
   bip15xTransport->setKeyCb(cbApproveProxy_);

   auto wsConnection = std::make_unique<WsDataConnection>(logger, WsDataConnectionParams{});
   auto connection = std::make_unique<Bip15xDataConnection>(logger, std::move(wsConnection), bip15xTransport);
   auto env = static_cast<ApplicationSettings::EnvConfiguration>(
            applicationSettings_->get<int>(ApplicationSettings::envConfiguration));
   bool result = connection->openConnection(PubKeyLoader::serverHostName(PubKeyLoader::KeyType::Proxy, env)
      , PubKeyLoader::serverHttpPort(), bsClient.get());
   assert(result);
   bsClient->setConnection(std::move(connection));

   // Must be connected before loginDialog.exec call (balances could be received before loginDialog.exec returns)!
   connect(bsClient.get(), &BsClientQt::balanceLoaded, assetManager_.get(), &AssetManager::fxBalanceLoaded);
   connect(bsClient.get(), &BsClientQt::balanceUpdated, assetManager_.get(), &AssetManager::onAccountBalanceLoaded);

   return bsClient;
}

void BSTerminalMainWindow::activateClient(const std::shared_ptr<BsClientQt> &bsClient
   , const BsClientLoginResult &result, const std::string &email)
{
   currentUserLogin_ = QString::fromStdString(email);

   chatTokenData_ = result.chatTokenData;
   chatTokenSign_ = result.chatTokenSign;
   tryLoginIntoChat();

   bsClient_ = bsClient;
   ccFileManager_->setBsClient(bsClient);
   authAddrDlg_->setBsClient(bsClient);

   tradeSettings_ = std::make_shared<bs::TradeSettings>(result.tradeSettings);
   applicationSettings_->set(ApplicationSettings::SubmittedAddressXbtLimit, static_cast<quint64>(tradeSettings_->xbtTier1Limit));

   authManager_->initLogin(celerConnection_, tradeSettings_);

   onBootstrapDataLoaded(result.bootstrapDataSigned);

   connect(bsClient_.get(), &BsClientQt::disconnected, orderListModel_.get(), &OrderListModel::onDisconnected);
   connect(bsClient_.get(), &BsClientQt::disconnected, this, &BSTerminalMainWindow::onBsConnectionDisconnected);
   connect(bsClient_.get(), &BsClientQt::connectionFailed, this, &BSTerminalMainWindow::onBsConnectionFailed);

   // connect to RFQ dialog
   connect(bsClient_.get(), &BsClientQt::processPbMessage, ui_->widgetRFQ, &RFQRequestWidget::onMessageFromPB);
   connect(bsClient_.get(), &BsClientQt::disconnected, ui_->widgetRFQ, &RFQRequestWidget::onUserDisconnected);
   connect(ui_->widgetRFQ, &RFQRequestWidget::sendUnsignedPayinToPB, bsClient_.get(), &BsClientQt::sendUnsignedPayin);
   connect(ui_->widgetRFQ, &RFQRequestWidget::sendSignedPayinToPB, bsClient_.get(), &BsClientQt::sendSignedPayin);
   connect(ui_->widgetRFQ, &RFQRequestWidget::sendSignedPayoutToPB, bsClient_.get(), &BsClientQt::sendSignedPayout);

   connect(ui_->widgetRFQ, &RFQRequestWidget::cancelXBTTrade, bsClient_.get(), &BsClientQt::sendCancelOnXBTTrade);
   connect(ui_->widgetRFQ, &RFQRequestWidget::cancelCCTrade, bsClient_.get(), &BsClientQt::sendCancelOnCCTrade);

   // connect to quote dialog
   connect(bsClient_.get(), &BsClientQt::processPbMessage, ui_->widgetRFQReply, &RFQReplyWidget::onMessageFromPB);
   connect(ui_->widgetRFQReply, &RFQReplyWidget::sendUnsignedPayinToPB, bsClient_.get(), &BsClientQt::sendUnsignedPayin);
   connect(ui_->widgetRFQReply, &RFQReplyWidget::sendSignedPayinToPB, bsClient_.get(), &BsClientQt::sendSignedPayin);
   connect(ui_->widgetRFQReply, &RFQReplyWidget::sendSignedPayoutToPB, bsClient_.get(), &BsClientQt::sendSignedPayout);

   connect(ui_->widgetRFQReply, &RFQReplyWidget::cancelXBTTrade, bsClient_.get(), &BsClientQt::sendCancelOnXBTTrade);
   connect(ui_->widgetRFQReply, &RFQReplyWidget::cancelCCTrade, bsClient_.get(), &BsClientQt::sendCancelOnCCTrade);

   connect(ui_->widgetChat, &ChatWidget::emailHashRequested, bsClient_.get(), &BsClientQt::findEmailHash);
   connect(bsClient_.get(), &BsClientQt::emailHashReceived, ui_->widgetChat, &ChatWidget::onEmailHashReceived);

   connect(bsClient_.get(), &BsClientQt::processPbMessage, orderListModel_.get(), &OrderListModel::onMessageFromPB);

   utxoReservationMgr_->setFeeRatePb(result.feeRatePb);
   connect(bsClient_.get(), &BsClientQt::feeRateReceived, this, [this] (float feeRate) {
      utxoReservationMgr_->setFeeRatePb(feeRate);
   });

   setLoginButtonText(currentUserLogin_);
   setWidgetsAuthorized(true);

   // We don't use password here, BsProxy will manage authentication
   SPDLOG_LOGGER_DEBUG(logMgr_->logger(), "got celer login: {}", result.celerLogin);
   celerConnection_->LoginToServer(bsClient_.get(), result.celerLogin, email);

   ui_->widgetWallets->setUsername(currentUserLogin_);
   action_logout_->setVisible(false);
   action_login_->setEnabled(false);

   // Market data, charts and chat should be available for all Auth eID logins
   mdProvider_->SubscribeToMD();

   connect(bsClient_.get(), &BsClientQt::processPbMessage, ui_->widgetChat, &ChatWidget::onProcessOtcPbMessage);
   connect(ui_->widgetChat, &ChatWidget::sendOtcPbMessage, bsClient_.get(), &BsClientQt::sendPbMessage);

   connect(bsClient_.get(), &BsClientQt::bootstrapDataUpdated, this, [this](const std::string& data) {
      onBootstrapDataLoaded(data);
   });

   accountEnabled_ = true;
   onAccountTypeChanged(result.userType, result.enabled);
   connect(bsClient_.get(), &BsClientQt::accountStateChanged, this, [this](bs::network::UserType userType, bool enabled) {
      onAccountTypeChanged(userType, enabled);
   });

   connect(bsClient_.get(), &BsClientQt::tradingStatusChanged, this, [](bool tradingEnabled) {
      NotificationCenter::notify(tradingEnabled ? bs::ui::NotifyType::TradingEnabledOnPB : bs::ui::NotifyType::TradingDisabledOnPB, {});
   });
}

const std::string &BSTerminalMainWindow::loginApiKeyEncrypted() const
{
   return loginApiKeyEncrypted_;
}

void BSTerminalMainWindow::initApiKeyLogins()
{
   if (loginTimer_ || loginApiKeyEncrypted().empty() || !gotChatKeys_) {
      return;
   }
   loginTimer_ = new QTimer(this);
   connect(loginTimer_, &QTimer::timeout, this, [this] {
      tryLoginUsingApiKey();
   });
   tryLoginUsingApiKey();
   loginTimer_->start(kAutoLoginTimer);
}

void BSTerminalMainWindow::tryLoginUsingApiKey()
{
   if (loginApiKeyEncrypted().empty() || autoLoginState_ != AutoLoginState::Idle) {
      return;
   }

   auto logger = logMgr_->logger("proxy");
   autoLoginClient_ = createClient();

   auto apiKeyErrorCb = [this, logger](AutoLoginState newState, const QString &errorMsg) {
      // Do not show related errors multiple times
      if (autoLoginState_ == AutoLoginState::Idle || autoLoginState_ == AutoLoginState::Failed) {
         return;
      }
      SPDLOG_LOGGER_ERROR(logger, "authorization failed: {}", errorMsg.toStdString());
      autoLoginState_ = newState;
      autoLoginClient_ = nullptr;
      if (autoLoginLastErrorMsg_ != errorMsg) {
         autoLoginLastErrorMsg_ = errorMsg;
         BSMessageBox(BSMessageBox::critical, tr("API key login")
            , tr("Login failed")
            , errorMsg
            , this).exec();
      }
   };

   connect(autoLoginClient_.get(), &BsClientQt::connected, this, [this, logger, apiKeyErrorCb] {
      connect(autoLoginClient_.get(), &BsClientQt::authorizeDone, this, [this, logger, apiKeyErrorCb]
            (BsClientCallbackTarget::AuthorizeError error, const std::string &email) {
         if (error != BsClientCallbackTarget::AuthorizeError::NoError) {
            switch (error) {
               case BsClientCallbackTarget::AuthorizeError::UnknownIpAddr:
                  apiKeyErrorCb(AutoLoginState::Failed, tr("Unexpected IP address"));
                  break;
               case BsClientCallbackTarget::AuthorizeError::UnknownApiKey:
                  apiKeyErrorCb(AutoLoginState::Failed, tr("API key not found"));
                  break;
               case BsClientCallbackTarget::AuthorizeError::Timeout:
                  apiKeyErrorCb(AutoLoginState::Idle, tr("Request timeout"));
                  break;
               default:
                  apiKeyErrorCb(AutoLoginState::Idle, tr("Unknown server error"));
                  break;
            }
            return;
         }

         connect(autoLoginClient_.get(), &BsClientQt::getLoginResultDone, this, [this, logger, email, apiKeyErrorCb]
               (const BsClientLoginResult &result) {
            if (result.status != AutheIDClient::NoError) {
               apiKeyErrorCb(AutoLoginState::Idle, tr("Login failed"));
               return;
            }
            activateClient(autoLoginClient_, result, email);
            autoLoginState_ = AutoLoginState::Connected;
            autoLoginClient_ = nullptr;
            autoLoginLastErrorMsg_.clear();
         });
         autoLoginClient_->getLoginResult();
      });

      SecureBinaryData apiKeyEncCopy;
      try {
         apiKeyEncCopy = SecureBinaryData::CreateFromHex(loginApiKeyEncrypted());
      } catch (...) {
         apiKeyErrorCb(AutoLoginState::Failed, tr("Encrypted API key invalid"));
         return;
      }

      ConfigDialog::decryptData(walletsMgr_, signContainer_, apiKeyEncCopy, [this, apiKeyErrorCb]
         (ConfigDialog::EncryptError error, const SecureBinaryData &data) {
         if (error != ConfigDialog::EncryptError::NoError) {
            apiKeyErrorCb(AutoLoginState::Failed, ConfigDialog::encryptErrorStr(error));
            return;
         }
         autoLoginClient_->authorize(data.toBinStr());
      });
   });

   connect(autoLoginClient_.get(), &BsClientQt::disconnected, this, [logger, apiKeyErrorCb] {
      apiKeyErrorCb(AutoLoginState::Idle, tr("Proxy disconnected"));
   });
   connect(autoLoginClient_.get(), &BsClientQt::connectionFailed, this, [logger, apiKeyErrorCb] {
      apiKeyErrorCb(AutoLoginState::Idle, tr("Proxy connection failed"));
   });

   autoLoginState_ = AutoLoginState::Connecting;
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

void BSTerminalMainWindow::openURIDialog()
{
   const bool testnetNetwork = applicationSettings_->get<NetworkType>(ApplicationSettings::netType) == NetworkType::TestNet;

   auto uiLogger = logMgr_->logger("ui");

   OpenURIDialog dlg{connectionManager_->GetNAM(), testnetNetwork, uiLogger, this};
   if (dlg.exec() == QDialog::Accepted) {
      // open create transaction dialog

      const auto requestInfo = dlg.getRequestInfo();
      std::shared_ptr<CreateTransactionDialog> cerateTxDlg;

      if (applicationSettings_->get<bool>(ApplicationSettings::AdvancedTxDialogByDefault)) {
         cerateTxDlg = CreateTransactionDialogAdvanced::CreateForPaymentRequest(armory_, walletsMgr_
            , utxoReservationMgr_, signContainer_, uiLogger, applicationSettings_
            , requestInfo, this);
      } else {
         cerateTxDlg = CreateTransactionDialogSimple::CreateForPaymentRequest(armory_, walletsMgr_
            , utxoReservationMgr_, signContainer_, uiLogger, applicationSettings_
            , requestInfo, this);
      }

      DisplayCreateTransactionDialog(cerateTxDlg);
   }
}

void BSTerminalMainWindow::DisplayCreateTransactionDialog(std::shared_ptr<CreateTransactionDialog> dlg)
{
   while(true) {
      dlg->exec();

      if  ((dlg->result() != QDialog::Accepted) || !dlg->switchModeRequested()) {
         break;
      }

      auto nextDialog = dlg->SwitchMode();
      dlg = nextDialog;
   }
}

void BSTerminalMainWindow::onBootstrapDataLoaded(const std::string& data)
{
   if (bootstrapDataManager_->setReceivedData(data)) {
      authManager_->SetLoadedValidationAddressList(bootstrapDataManager_->GetAuthValidationList());
      ccFileManager_->SetLoadedDefinitions(bootstrapDataManager_->GetCCDefinitions());
   }
}

void BSTerminalMainWindow::onAuthLeafCreated()
{
   auto authWallet = walletsMgr_->getAuthWallet();
   if (authWallet != nullptr) {
      // check that current wallet has auth address that was submitted at some point
      // if there is no such address - display auth address dialog, so user could submit
      auto submittedAddresses = celerConnection_->GetSubmittedAuthAddressSet();
      auto existingAddresses = authWallet->getUsedAddressList();

      bool haveSubmittedAddress = false;
      for ( const auto& address : existingAddresses) {
         if (submittedAddresses.find(address.display()) != submittedAddresses.end()) {
            haveSubmittedAddress = true;
            break;
         }
      }

      if (!haveSubmittedAddress) {
         addDeferredDialog([this]()
                           {
                              openAuthManagerDialog();
                           });
      }
   }
}
