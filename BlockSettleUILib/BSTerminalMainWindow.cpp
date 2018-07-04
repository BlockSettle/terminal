#include "BSTerminalMainWindow.h"
#include "ui_BSTerminalMainWindow.h"
#include "moc_BSTerminalMainWindow.cpp"

#include <QApplication>
#include <QCloseEvent>
#include <QDebug>
#include <QGuiApplication>
#include <QIcon>
#include <QShortcut>
#include <QStringList>
#include <QSystemTrayIcon>
#include <QtConcurrent/QtConcurrentRun>
#include <QToolBar>
#include <QTreeView>

#include <thread>

#include "AboutDialog.h"
#include "AssetManager.h"
#include "AuthAddressDialog.h"
#include "AuthAddressManager.h"
#include "BSTerminalSplashScreen.h"
#include "CCFileManager.h"
#include "CCPortfolioModel.h"
#include "CCTokenEntryDialog.h"
#include "CelerAccountInfoDialog.h"
#include "CelerClient.h"
#include "ConfigDialog.h"
#include "ConnectionManager.h"
#include "CreateTransactionDialogAdvanced.h"
#include "CreateTransactionDialogSimple.h"
#include "DialogManager.h"
#include "EnterWalletPassword.h"
#include "HDWallet.h"
#include "HeadlessContainer.h"
#include "LoginWindow.h"
#include "MarketDataProvider.h"
#include "MessageBoxCritical.h"
#include "MessageBoxInfo.h"
#include "MessageBoxQuestion.h"
#include "MessageBoxSuccess.h"
#include "MessageBoxWarning.h"
#include "NewAddressDialog.h"
#include "NewWalletDialog.h"
#include "NotificationCenter.h"
#include "OTPFileInfoDialog.h"
#include "OTPImportDialog.h"
#include "OTPManager.h"
#include "QuoteProvider.h"
#include "SelectWalletDialog.h"
#include "SignContainer.h"
#include "StatusBarView.h"
#include "UiUtils.h"
#include "WalletsManager.h"
#include "ZmqSecuredDataConnection.h"
#include "TabWithShortcut.h"

#include <spdlog/spdlog.h>

class MainBlockListener : public PyBlockDataListener
{
public:
   MainBlockListener(BSTerminalMainWindow* parent)
      : mainWindow_(parent)
   {}

   ~MainBlockListener() noexcept override = default;

   void StateChanged(PyBlockDataManagerState newState) override {
      mainWindow_->onBDMStateChanged(newState);
   }

private:
   BSTerminalMainWindow *mainWindow_;
};

BSTerminalMainWindow::BSTerminalMainWindow(const std::shared_ptr<ApplicationSettings>& settings, BSTerminalSplashScreen& splashScreen, QWidget* parent)
   : QMainWindow(parent)
   , ui(new Ui::BSTerminalMainWindow())
   , applicationSettings_(settings)
   , walletsManager_(nullptr)
   , bdm_(nullptr)
   , bdmListener_(nullptr)
{
   UiUtils::SetupLocale();

   ui->setupUi(this);

   setupShortcuts();

   if (!applicationSettings_->get<bool>(ApplicationSettings::initialized)) {
      applicationSettings_->SetDefaultSettings(true);
   }

   auto geom = settings->get<QRect>(ApplicationSettings::GUI_main_geometry);
   if (!geom.isEmpty()) {
      setGeometry(geom);
   }

   qRegisterMetaType<PyBlockDataManagerState>();
   qRegisterMetaType<LedgerEntryData>();
   qRegisterMetaType<std::vector<LedgerEntryData> >();
   qRegisterMetaType<std::vector<UTXO> >();
   connect(ui->action_Quit, &QAction::triggered, qApp, &QCoreApplication::quit);

   logMgr_ = std::make_shared<bs::LogManager>([] { KillHeadlessProcess(); });
   logMgr_->add(applicationSettings_->GetLogsConfig());

   logMgr_->logger()->debug("Settings loaded from {}", applicationSettings_->GetSettingsPath().toStdString());

   setupIcon();
   UiUtils::setupIconFont(this);
   NotificationCenter::createInstance(applicationSettings_, ui, sysTrayIcon_, this);

   InitConnections();

   bdm_ = PyBlockDataManager::createDataManager(applicationSettings_->GetArmorySettings()
      , applicationSettings_->get<std::string>(ApplicationSettings::txCacheFileName));
   if (bdm_) {
      PyBlockDataManager::setInstance(bdm_);
      connect(bdm_.get(), &PyBlockDataManager::txBroadcastError, [](const QString &txHash, const QString &error) {
         NotificationCenter::notify(bs::ui::NotifyType::BroadcastError, { txHash, error });
      });
   }
   else {
      logMgr_->logger()->error("Failed to create BlockDataManager");
   }

   otpManager_ = std::make_shared<OTPManager>(logMgr_->logger(), applicationSettings_, celerConnection_);
   connect(otpManager_.get(), &OTPManager::SyncCompleted, this, &BSTerminalMainWindow::OnOTPSyncCompleted);

   InitSigningContainer();
   LoadWallets(splashScreen);

   InitAuthManager();
   InitAssets();

   authAddrDlg_ = std::make_shared<AuthAddressDialog>(authManager_, assetManager_, applicationSettings_, this);

   InitWalletsView();
   setupToolbar();
   setupMenu();
   setupStatusBar();

   ui->widgetTransactions->setEnabled(false);

   if (!signContainer_->Start()) {
      MessageBoxWarning(tr("BlockSettle Signer"), tr("Failed to start local signer process")).exec();
   }

   setupBDM();
   splashScreen.SetProgress(100);

   InitPortfolioView();

   aboutDlg_ = std::make_shared<AboutDialog>(applicationSettings_->get<QString>(ApplicationSettings::ChangeLog_Base_Url), this);
   auto aboutDlgCb = [this] (int tab) {
      return [this, tab]() {
         aboutDlg_->setTab(tab);
         aboutDlg_->show();
      };
   };
   connect(ui->action_About_BlockSettle, &QAction::triggered, aboutDlgCb(0));
   connect(ui->actionAbout_the_Terminal, &QAction::triggered, aboutDlgCb(1));
   connect(ui->action_Contact_BlockSettle, &QAction::triggered, aboutDlgCb(2));
   connect(ui->action_Version, &QAction::triggered, aboutDlgCb(3));

   ui->tabWidget->setCurrentIndex(settings->get<int>(ApplicationSettings::GUI_main_tab));

   UpdateMainWindowAppearence();
}

BSTerminalMainWindow::~BSTerminalMainWindow()
{
   applicationSettings_->set(ApplicationSettings::GUI_main_geometry, geometry());
   applicationSettings_->set(ApplicationSettings::GUI_main_tab, ui->tabWidget->currentIndex());
   applicationSettings_->SaveSettings();

   NotificationCenter::destroyInstance();
   if (bdm_) {
       bdm_->removeListener(bdmListener_.get());
   }
   if (signContainer_) {
      signContainer_->Stop();
   }
   walletsManager_ = nullptr;
   assetManager_ = nullptr;
   PyBlockDataManager::setInstance(nullptr);
   bs::UtxoReservation::destroy();
}

void BSTerminalMainWindow::setupToolbar()
{
   QIcon lockbox_icon = UiUtils::icon(0xe774);
   QIcon offline_icon = UiUtils::icon(0xe77f);
   QIcon create_wallet_icon = UiUtils::icon(0xe67a);
   QIcon wallet_properties_icon = UiUtils::icon(0xe6c3);
   QIcon import_wallet_icon = UiUtils::icon(0xe765);

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
   ui->tabWidget->setCornerWidget(toolBar, Qt::TopRightCorner);

   // send bitcoins
   toolBar->addAction(action_send_);
   // receive bitcoins
   toolBar->addAction(action_receive_);

   action_send_->setEnabled(false);
   action_logout_->setVisible(false);

   QMenu* userMenu = new QMenu(this);
   userMenu->addAction(action_login_);
   userMenu->addAction(action_logout_);
   ui->pushButtonUser->setMenu(userMenu);

   QMenu* trayMenu = new QMenu(this);
   QAction* trayShowAction = trayMenu->addAction(tr("&Open Terminal"));
   connect(trayShowAction, &QAction::triggered, this, &QMainWindow::show);
   trayMenu->addSeparator();

   trayMenu->addAction(action_send_);
   trayMenu->addAction(action_receive_);
   trayMenu->addAction(ui->actionSettings);

   trayMenu->addSeparator();
   trayMenu->addAction(ui->action_Quit);
   sysTrayIcon_->setContextMenu(trayMenu);

   if (bdm_) {
      connect(bdm_.get(), &PyBlockDataManager::zeroConfReceived, this, &BSTerminalMainWindow::showZcNotification);
   }
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

void BSTerminalMainWindow::LoadWallets(BSTerminalSplashScreen& splashScreen)
{
   logMgr_->logger()->debug("Loading wallets");
   splashScreen.SetTipText(tr("Loading wallets"));
   splashScreen.SetProgress(5);

   bs::UtxoReservation::init();

   WalletsManager::load_progress_delegate progressDelegate = [&](int progress)
   {
      splashScreen.SetProgress(progress);
   };

   walletsManager_ = std::make_shared<WalletsManager>(logMgr_->logger(), applicationSettings_, bdm_);

   connect(walletsManager_.get(), &WalletsManager::walletsReady, [this] {
      ui->widgetRFQ->SetWalletsManager(walletsManager_);
      ui->widgetRFQReply->SetWalletsManager(walletsManager_);
   });
   connect(walletsManager_.get(), &WalletsManager::info, this, &BSTerminalMainWindow::showInfo);
   connect(walletsManager_.get(), &WalletsManager::error, this, &BSTerminalMainWindow::showError);

   walletsManager_->LoadWallets(applicationSettings_->get<NetworkType>(ApplicationSettings::netType)
      , applicationSettings_->GetHomeDir(), progressDelegate);

   if (signContainer_->opMode() != SignContainer::OpMode::Offline) {
      addrSyncer_ = std::make_shared<HeadlessAddressSyncer>(signContainer_, walletsManager_);
      connect(signContainer_.get(), &SignContainer::UserIdSet, [this] {
         addrSyncer_->SyncWallet(walletsManager_->GetAuthWallet());
      });
   }

   logMgr_->logger()->debug("End of wallets loading");
}

void BSTerminalMainWindow::InitAuthManager()
{
   authManager_ = std::make_shared<AuthAddressManager>(logMgr_->logger());
   authManager_->init(applicationSettings_, walletsManager_, otpManager_);
   authManager_->SetSigningContainer(signContainer_);

   connect(authManager_.get(), &AuthAddressManager::NeedVerify, this, &BSTerminalMainWindow::openAuthDlgVerify);
   connect(authManager_.get(), &AuthAddressManager::AddrStateChanged, [](const QString &addr, const QString &state) {
      NotificationCenter::notify(bs::ui::NotifyType::AuthAddress, { addr, state });
   });
   connect(authManager_.get(), &AuthAddressManager::ConnectionComplete, this, &BSTerminalMainWindow::onAuthMgrConnComplete);
   connect(authManager_.get(), &AuthAddressManager::AuthWalletCreated, [this](const QString &walletId) {
      if (authAddrDlg_) {
         openAuthManagerDialog();
      }
   });
}

void BSTerminalMainWindow::InitSigningContainer()
{
   signContainer_ = CreateSigner(logMgr_->logger(), applicationSettings_);
   if (!signContainer_) {
      showError(tr("Signer"), tr("Creation failure"));
      return;
   }
   connect(signContainer_.get(), &SignContainer::ready, this, &BSTerminalMainWindow::SignerReady);
}

void BSTerminalMainWindow::SignerReady()
{
   if (signContainer_->hasUI()) {
      disconnect(signContainer_.get(), &SignContainer::PasswordRequested, this, &BSTerminalMainWindow::onPasswordRequested);
   }
   else {
      connect(signContainer_.get(), &SignContainer::PasswordRequested, this, &BSTerminalMainWindow::onPasswordRequested);
   }

   if (!widgetsInited_) {
      auto quoteProvider = std::make_shared<QuoteProvider>(assetManager_, logMgr_->logger("message"));
      quoteProvider->ConnectToCelerClient(celerConnection_);

      auto dialogManager = std::make_shared<DialogManager>(geometry());

      ui->widgetRFQ->init(logMgr_->logger(), celerConnection_, authManager_, quoteProvider, mdProvider_, assetManager_
         , applicationSettings_, dialogManager, signContainer_);
      ui->widgetRFQReply->init(logMgr_->logger(), celerConnection_, authManager_, quoteProvider, mdProvider_, assetManager_
         , applicationSettings_, dialogManager, signContainer_);
      widgetsInited_ = true;
   }
   else {
      signContainer_->SetUserId(BinaryData::CreateFromHex(celerConnection_->userId()));
   }
}

void BSTerminalMainWindow::InitConnections()
{
   connectionManager_ = std::make_shared<ConnectionManager>(logMgr_->logger("message"));
   celerConnection_ = std::make_shared<CelerClient>(connectionManager_);
   connect(celerConnection_.get(), &CelerClient::OnConnectedToServer, this, &BSTerminalMainWindow::onCelerConnected);
   connect(celerConnection_.get(), &CelerClient::OnConnectionClosed, this, &BSTerminalMainWindow::onCelerDisconnected);
   connect(celerConnection_.get(), &CelerClient::OnConnectionError, this, &BSTerminalMainWindow::onCelerConnectionError, Qt::QueuedConnection);

   mdProvider_ = std::make_shared<MarketDataProvider>(logMgr_->logger("message"));
   mdProvider_->ConnectToCelerClient(celerConnection_, true);
}

void BSTerminalMainWindow::InitAssets()
{
   ccFileManager_ = std::make_shared<CCFileManager>(logMgr_->logger(), applicationSettings_, otpManager_);
   assetManager_ = std::make_shared<AssetManager>(logMgr_->logger(), walletsManager_, mdProvider_, celerConnection_);
   assetManager_->init();

   connect(ccFileManager_.get(), &CCFileManager::CCSecurityDef, assetManager_.get(), &AssetManager::onCCSecurityReceived);
   connect(ccFileManager_.get(), &CCFileManager::CCSecurityInfo, walletsManager_.get(), &WalletsManager::onCCSecurityInfo);
   connect(ccFileManager_.get(), &CCFileManager::Loaded, walletsManager_.get(), &WalletsManager::onCCInfoLoaded);
   connect(ccFileManager_.get(), &CCFileManager::LoadingFailed, this, &BSTerminalMainWindow::onCCInfoMissing);
   connect(mdProvider_.get(), &MarketDataProvider::MDUpdate, assetManager_.get(), &AssetManager::onMDUpdate);
   ccFileManager_->LoadData();
}

void BSTerminalMainWindow::InitPortfolioView()
{
   portfolioModel_ = std::make_shared<CCPortfolioModel>(walletsManager_, assetManager_, this);
   ui->widgetPortfolio->ConnectToMD(applicationSettings_, mdProvider_);
   ui->widgetPortfolio->SetPortfolioModel(portfolioModel_);
   ui->widgetPortfolio->SetSigningContainer(signContainer_);
}

void BSTerminalMainWindow::InitWalletsView()
{
   ui->widgetWallets->init(walletsManager_, signContainer_, applicationSettings_, assetManager_, authManager_);
}

void BSTerminalMainWindow::setupStatusBar()
{
   statusBarView_ = new StatusBarView(bdm_, walletsManager_, assetManager_, ui->statusbar);
   statusBarView_->connectToCelerClient(celerConnection_);
   statusBarView_->connectToContainer(signContainer_);
}

void BSTerminalMainWindow::InitTransactionsView()
{
   ui->widgetTransactions->setEnabled(true);

   ui->widgetTransactions->SetTransactionsModel(transactionsModel_);
   ui->widgetPortfolio->SetTransactionsModel(transactionsModel_);
}

void BSTerminalMainWindow::BDMStateChanged(PyBlockDataManagerState newState)
{
   switch(newState)
   {
   case PyBlockDataManagerState::Ready:
      CompleteUIOnlineView();
      break;
   case PyBlockDataManagerState::Connected:
      CompleteDBConnection();
      break;
   case PyBlockDataManagerState::Offline:
      SetOfflineUIView();
      break;
   case PyBlockDataManagerState::Scaning:
      break;
   case PyBlockDataManagerState::Error:
      break;
   case PyBlockDataManagerState::Closing:
      break;
   }
}

void BSTerminalMainWindow::CompleteUIOnlineView()
{
   auto walletsDelegate = bdm_->GetWalletsLedgerDelegate();

   if (walletsDelegate == nullptr) {
      logMgr_->logger("ui")->error("[CompleteUIOnlineView] failed to create wallets delegate. Go to offline mode");
      return;
   }

   transactionsModel_ = std::make_shared<TransactionsViewModel>(bdm_, walletsManager_, walletsDelegate, this);

   QMetaObject::invokeMethod(this, "InitTransactionsView", Qt::QueuedConnection);

   if (walletsManager_->GetWalletsCount() != 0) {
       action_send_->setEnabled(true);
   } else {
      QTimer::singleShot(1234, [this] { createWallet(!walletsManager_->HasPrimaryWallet()); });
   }
}

void BSTerminalMainWindow::CompleteDBConnection()
{
   qDebug() << "BSTerminalMainWindow::CompleteDBConnection";

   walletsManager_->RegisterSavedWallets();
}

void BSTerminalMainWindow::onReactivate()
{
   show();
}

void BSTerminalMainWindow::UpdateMainWindowAppearence()
{
   if (!applicationSettings_->get<bool>(ApplicationSettings::closeToTray) && isHidden()) {
      setWindowState(windowState() & ~Qt::WindowMinimized);
      show();
      raise();
      activateWindow();
   }

   const auto bsTitle = tr("BlockSettle Terminal [%1]");
   switch (applicationSettings_->get<NetworkType>(ApplicationSettings::netType)) {
   case NetworkType::TestNet:
      setWindowTitle(bsTitle.arg(tr("TESTNET")));
      break;

   case NetworkType::RegTest:
      setWindowTitle(bsTitle.arg(tr("REGTEST")));
      break;

   default:
      setWindowTitle(tr("BlockSettle Terminal"));
      break;
   }
}

void BSTerminalMainWindow::SetOfflineUIView()
{
   action_send_->setEnabled(false);
}

void BSTerminalMainWindow::setupBDM()
{
   bdmListener_ = std::make_shared<MainBlockListener>(this);
   connect(this, &BSTerminalMainWindow::onBDMStateChanged, this, &BSTerminalMainWindow::BDMStateChanged);

   if (bdm_) {
      bdm_->addListener(bdmListener_.get());
      QtConcurrent::run(bdm_.get(), &PyBlockDataManager::setupConnection);
   }
}

bool BSTerminalMainWindow::createWallet(bool primary, bool reportSuccess)
{
   if (primary && (walletsManager_->GetHDWalletsCount() > 0)) {
      auto wallet = walletsManager_->GetHDWallet(0);
      if (wallet->isPrimary()) {
         return true;
      }
      MessageBoxQuestion qry(tr("Create primary wallet"), tr("Promote to primary wallet")
         , tr("In order to execute trades and take delivery of XBT and Equity Tokens, you are required to"
            " have a Primary Wallet which supports the sub-wallets required to interact with the system.")
         .arg(QString::fromStdString(wallet->getName())), this);
      if (qry.exec() == QDialog::Accepted) {
         wallet->createGroup(bs::hd::CoinType::BlockSettle_Auth);
         return true;
      }
      return false;
   }
   NewWalletDialog newWalletDialog(true, this);
   if (!newWalletDialog.exec()) {
      return false;
   }

   if (newWalletDialog.isCreate()) {
      return ui->widgetWallets->CreateNewWallet(primary, reportSuccess);
   }
   else if (newWalletDialog.isImport()) {
      return ui->widgetWallets->ImportNewWallet(primary, reportSuccess);
   }
   return false;
}

void BSTerminalMainWindow::showInfo(const QString &title, const QString &text)
{
   MessageBoxInfo(title, text).exec();
}

void BSTerminalMainWindow::showError(const QString &title, const QString &text)
{
   MessageBoxCritical(title, text, this).exec();
}

void BSTerminalMainWindow::onReceive()
{
   const auto &defWallet = walletsManager_->GetDefaultWallet();
   std::string selWalletId = defWallet ? defWallet->GetWalletId() : std::string{};
   if (ui->tabWidget->currentWidget() == ui->widgetWallets) {
      const auto &wallets = ui->widgetWallets->GetSelectedWallets();
      if (wallets.size() == 1) {
         selWalletId = wallets[0]->GetWalletId();
      }
   }
   SelectWalletDialog *selectWalletDialog = new SelectWalletDialog(walletsManager_, selWalletId, this);
   selectWalletDialog->exec();

   if (selectWalletDialog->result() == QDialog::Rejected) {
      return;
   }

   NewAddressDialog* newAddressDialog = new NewAddressDialog(selectWalletDialog->getSelectedWallet()
      , signContainer_, selectWalletDialog->isNestedSegWitAddress(), this);
   newAddressDialog->show();
}

void BSTerminalMainWindow::onSend()
{
   std::string selectedWalletId;

   if (ui->tabWidget->currentWidget() == ui->widgetWallets) {
      const auto &wallets = ui->widgetWallets->GetSelectedWallets();
      if (wallets.size() == 1) {
         selectedWalletId = wallets[0]->GetWalletId();
      }
   }

   if (QGuiApplication::keyboardModifiers() & Qt::ShiftModifier) {
      CreateTransactionDialogAdvanced advancedDialog{walletsManager_, signContainer_, true, this};
      advancedDialog.setOfflineDir(applicationSettings_->get<QString>(ApplicationSettings::signerOfflineDir));

      if (!selectedWalletId.empty()) {
         advancedDialog.SelectWallet(selectedWalletId);
      }

      advancedDialog.exec();
   } else {
      CreateTransactionDialogSimple dlg{walletsManager_, signContainer_, this};
      dlg.setOfflineDir(applicationSettings_->get<QString>(ApplicationSettings::signerOfflineDir));

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

void BSTerminalMainWindow::setupMenu()
{
   connect(ui->action_Create_New_Wallet, &QAction::triggered, [ww = ui->widgetWallets]{ ww->CreateNewWallet(false); });
   connect(ui->actionAuthentication_Addresses, &QAction::triggered, this, &BSTerminalMainWindow::openAuthManagerDialog);
   connect(ui->action_One_time_Password, &QAction::triggered, this, &BSTerminalMainWindow::openOTPDialog);
   connect(ui->actionSettings, &QAction::triggered, this, [=]() { openConfigDialog(); });
   connect(ui->actionAccount_Information, &QAction::triggered, this, &BSTerminalMainWindow::openAccountInfoDialog);
   connect(ui->actionEnter_Color_Coin_token, &QAction::triggered, this, &BSTerminalMainWindow::openCCTokenDialog);

   onUserLoggedOut();

#ifndef Q_OS_MAC
   ui->horizontalFrame->hide();

   ui->menubar->setCornerWidget(ui->pushButtonUser);
#endif
}

void BSTerminalMainWindow::openOTPDialog()
{
   if (otpManager_->CurrentUserHaveOTP()) {
      OTPFileInfoDialog dialog(otpManager_, this);
      dialog.exec();
   } else {
      OTPImportDialog(otpManager_, this).exec();
   }
}

void BSTerminalMainWindow::openAuthManagerDialog()
{
   openAuthDlgVerify(QString());
}

void BSTerminalMainWindow::openAuthDlgVerify(const QString &addrToVerify)
{
   if (authManager_->HaveAuthWallet()) {
      authAddrDlg_->setAddressToVerify(addrToVerify);
      authAddrDlg_->show();
   }
}

void BSTerminalMainWindow::openConfigDialog()
{
   ConfigDialog configDialog(applicationSettings_, walletsManager_, assetManager_, this);
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
   if (!otpManager_->CurrentUserHaveOTP()) {
      MessageBoxQuestion createOtpReq(tr("One-Time Password")
         , tr("IMPORT ONE-TIME PASSWORD")
         , tr("Would you like to import your OTP at this time?")
         , tr("BlockSettle has sent a One-Time Password to your registered postal address. The OTP is used "
            "for confirming your identity and to establish secure channel through which communication can occur.")
         , this);
      if (createOtpReq.exec() == QDialog::Accepted) {
         OTPImportDialog otpDialog(otpManager_, this);
         if (otpDialog.exec() != QDialog::Accepted) {
            return;
         }
      }
      else {
         return;
      }
   }
   if (walletsManager_->HasPrimaryWallet() || createWallet(true, false)) {
      CCTokenEntryDialog dialog(walletsManager_, ccFileManager_, signContainer_, this);
      dialog.exec();
   }
}

void BSTerminalMainWindow::onLogin()
{
   LoginWindow loginDialog(applicationSettings_, this);

   if (loginDialog.exec() == QDialog::Accepted) {
      const std::string host = applicationSettings_->get<std::string>(ApplicationSettings::celerHost);
      const std::string port = applicationSettings_->get<std::string>(ApplicationSettings::celerPort);
      const std::string username = loginDialog.getUsername().toStdString();
      const std::string password = loginDialog.getPassword().toStdString();

      if (!celerConnection_->LoginToServer(host, port, username, password)) {
         logMgr_->logger("ui")->error("[BSTerminalMainWindow::onLogin] LoginToServer failed");
      } else {
         action_logout_->setVisible(false);
         action_login_->setEnabled(false);
      }
   }
}

void BSTerminalMainWindow::onLogout()
{
   celerConnection_->CloseConnection();
}

void BSTerminalMainWindow::onUserLoggedIn()
{
   ui->actionAccount_Information->setEnabled(true);
   ui->actionAuthentication_Addresses->setEnabled(true);
   ui->action_One_time_Password->setEnabled(true);
   ui->actionEnter_Color_Coin_token->setEnabled(true);

   ui->actionDeposits->setEnabled(true);
   ui->actionWithdrawal_Request->setEnabled(true);
   ui->actionLink_Additional_Bank_Account->setEnabled(true);

   authManager_->ConnectToPublicBridge(connectionManager_, celerConnection_);
   ccFileManager_->ConnectToPublicBridge(connectionManager_, celerConnection_);

   const auto userId = BinaryData::CreateFromHex(celerConnection_->userId());
   signContainer_->SetUserId(userId);
   walletsManager_->SetUserId(userId);

   setLoginButtonText(QString::fromStdString(celerConnection_->userName()));
}

void BSTerminalMainWindow::onUserLoggedOut()
{
   ui->actionAccount_Information->setEnabled(false);
   ui->actionAuthentication_Addresses->setEnabled(false);
   ui->actionEnter_Color_Coin_token->setEnabled(false);
   ui->action_One_time_Password->setEnabled(false);

   ui->actionDeposits->setEnabled(false);
   ui->actionWithdrawal_Request->setEnabled(false);
   ui->actionLink_Additional_Bank_Account->setEnabled(false);

   signContainer_->SetUserId(BinaryData{});
   walletsManager_->SetUserId(BinaryData{});
   authManager_->OnDisconnectedFromCeler();
   setLoginButtonText(tr("user.name"));
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
   case CelerClient::LoginError:
      MessageBoxCritical loginErrorBox(tr("Login failed"), tr("Invalid username/password pair"), this);
      loginErrorBox.exec();
      break;
   }
}

void BSTerminalMainWindow::onAuthMgrConnComplete()
{
   if (celerConnection_->tradingAllowed()) {
      if (!walletsManager_->HasPrimaryWallet() && !createWallet(true)) {
         return;
      }
      if (!walletsManager_->HasSettlementWallet()) {
         MessageBoxQuestion createSettlReq(tr("Create settlement wallet")
            , tr("Settlement wallet missing")
            , tr("You don't have Settlement wallet, yet. Do you wish to create it?")
            , this);
         if (createSettlReq.exec() == QDialog::Accepted) {
            const auto title = tr("Settlement wallet");
            if (walletsManager_->CreateSettlementWallet(applicationSettings_->get<NetworkType>(ApplicationSettings::netType)
               , applicationSettings_->GetHomeDir())) {
               MessageBoxSuccess(title, tr("Settlement wallet successfully created")).exec();
            } else {
               showError(title, tr("Failed to create"));
               return;
            }
         }
         else {
            return;
         }
      }

      if (authManager_->HaveOTP() && !walletsManager_->GetAuthWallet()) {
         MessageBoxQuestion createAuthReq(tr("Authentication Wallet")
            , tr("Create Authentication Wallet")
            , tr("You don't have a sub-wallet in which to hold Authentication Addresses. Would you like to create one?")
            , this);
         if (createAuthReq.exec() == QDialog::Accepted) {
            authManager_->CreateAuthWallet();
         }
      }
   }
   else {
      logMgr_->logger("ui")->debug("Trading not allowed");
   }
}

void BSTerminalMainWindow::showZcNotification(const std::vector<LedgerEntryData>& entries)
{
   if (entries.empty()) {
      return;
   }
   QStringList lines;
   for (const auto& led : entries) {
      const auto tx = bdm_->getTxByHash(led.getTxHash());
      const auto &wallet = walletsManager_->GetWalletById(led.getWalletID());
      if (!wallet) {
         continue;
      }

      lines << tr("Date: %1").arg(UiUtils::displayDateTime(led.getTxTime()));
      lines << tr("TX: %1 %2 %3").arg(tr(bs::Transaction::toString(walletsManager_->GetTransactionDirection(tx, wallet))))
         .arg(wallet->displayTxValue(led.getValue())).arg(wallet->displaySymbol());
      lines << tr("Wallet: %1").arg(QString::fromStdString(wallet->GetWalletName()));
      lines << UiUtils::displayAddress(walletsManager_->GetTransactionMainAddress(tx, wallet, (led.getValue() > 0)));
      lines << QLatin1String("");
   }
   if (lines.isEmpty()) {
      return;
   }
   const auto title = (entries.size() == 1) ? tr("New blockchain transaction")
      : tr("%1 new blockchain transactions").arg(QString::number(entries.size()));
   NotificationCenter::notify(bs::ui::NotifyType::BlockchainTX, { title, lines.join(tr("\n")) });
}

void BSTerminalMainWindow::showRunInBackgroundMessage()
{
   qDebug() << "showMessage" << sysTrayIcon_->isVisible();
   sysTrayIcon_->showMessage(tr("BlockSettle is running"), tr("BlockSettle Terminal is running in the backgroud. Click the tray icon to open the main window."), QSystemTrayIcon::Information);
}

void BSTerminalMainWindow::closeEvent(QCloseEvent* event)
{
   if (applicationSettings_->get<bool>(ApplicationSettings::closeToTray)) {
      hide();
      event->ignore();
   }
   else {
      QMainWindow::closeEvent(event);
      QApplication::exit();
      std::thread([] {
         std::this_thread::sleep_for(std::chrono::seconds(10));
         exit(0);
      }).detach();
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
   ui->pushButtonUser->setText(text);

#ifndef Q_OS_MAC
   ui->menubar->adjustSize();
#endif
}

void BSTerminalMainWindow::onPasswordRequested(std::string walletId, std::string prompt
   , bs::wallet::EncryptionType encType, SecureBinaryData encKey)
{
   SignContainer::PasswordType password;

   if (walletId.empty()) {
      logMgr_->logger("ui")->error("[onPasswordRequested] can\'t ask password for empty wallet id");
   } else {
      QString walletName;
      const auto wallet = walletsManager_->GetWalletById(walletId);
      if (wallet != nullptr) {
         // do we need to get name of root wallet?
         walletName = QString::fromStdString(wallet->GetWalletName());
      } else {
         const auto hdWallet = walletsManager_->GetHDWalletById(walletId);
         walletName = QString::fromStdString(hdWallet->getName());
      }

      if (!walletName.isEmpty()) {
         const auto &rootWallet = walletsManager_->GetHDRootForLeaf(walletId);
         EnterWalletPassword passwordDialog(walletName, rootWallet ? rootWallet->getWalletId() : walletId
            , encType, encKey, QString::fromStdString(prompt), this);
         if (passwordDialog.exec() == QDialog::Accepted) {
            password = passwordDialog.GetPassword();
         } else {
            logMgr_->logger("ui")->debug("[onPasswordRequested] user rejected to enter password for wallet {} ( {} )"
               , walletId, walletName.toStdString());
         }
      } else {
         logMgr_->logger("ui")->error("[onPasswordRequested] can\'t find wallet with id {}", walletId);
      }
   }

   signContainer_->SendPassword(walletId, password);
}

void BSTerminalMainWindow::OnOTPSyncCompleted()
{
   if (otpManager_->CurrentUserHaveOTP()) {
      if (!otpManager_->IsCurrentOTPLatest()) {
         MessageBoxQuestion removeOtpQuestion(tr("OTP outdated")
            , tr("Your OTP is outdated")
            , tr("Do you want to remove outdated OTP?")
            , tr("Looks like new OTP was generated for your account. All future requests signed by your local OTP will be rejected.")
            , this);

         if (removeOtpQuestion.exec() == QDialog::Accepted) {
            if (otpManager_->RemoveOTPForCurrentUser()) {
               MessageBoxInfo(tr("OTP file removed"), tr("Old OTP file was removed."), this).exec();
            } else {
               MessageBoxCritical(tr("OTP file not removed"), tr("Terminal failed to remove OTP file."), this).exec();
            }
         }
      } else if (otpManager_->CountAdvancingRequired()) {
         MessageBoxQuestion otpDialog(tr("One-Time Password")
            , tr("UPDATE ONE-TIME PASSWORD COUNTER")
            , tr("Would you like to update your OTP usage counter at this time?")
            , tr("Looks like you have used your OTP on another machine. Local OTP usage counter should be advanced or your requests will be rejected.")
            , this);

         if (otpDialog.exec() == QDialog::Accepted) {
            OTPFileInfoDialog(otpManager_, this).exec();
         }
      }
   } else if (celerConnection_->tradingAllowed() ) {
      MessageBoxQuestion createOtpReq(tr("One-Time Password")
         , tr("IMPORT ONE-TIME PASSWORD")
         , tr("Would you like to import your OTP at this time?")
         , tr("BlockSettle has sent a One-Time Password to your registered postal address. The OTP is used "
            "for confirming your identity and to establish secure channel through which communication can occur.")
         , this);
      if (createOtpReq.exec() == QDialog::Accepted) {
         OTPImportDialog(otpManager_, this).exec();
      }
   }
}

void BSTerminalMainWindow::onCCInfoMissing()
{ }   // do nothing here since we don't know if user will need Private Market before logon to Celer

void BSTerminalMainWindow::setupShortcuts()
{
   auto overviewTabShortcut = new QShortcut(QKeySequence(QString::fromStdString("Ctrl+1")), this);
   overviewTabShortcut->setContext(Qt::ApplicationShortcut);
   connect(overviewTabShortcut, &QShortcut::activated, [this](){ ui->tabWidget->setCurrentIndex(0);});

   auto tradingTabShortcut = new QShortcut(QKeySequence(QString::fromStdString("Ctrl+2")), this);
   tradingTabShortcut->setContext(Qt::ApplicationShortcut);
   connect(tradingTabShortcut, &QShortcut::activated, [this](){ ui->tabWidget->setCurrentIndex(1);});

   auto dealingTabShortcut = new QShortcut(QKeySequence(QString::fromStdString("Ctrl+3")), this);
   dealingTabShortcut->setContext(Qt::ApplicationShortcut);
   connect(dealingTabShortcut, &QShortcut::activated, [this](){ ui->tabWidget->setCurrentIndex(2);});

   auto walletsTabShortcutt = new QShortcut(QKeySequence(QString::fromStdString("Ctrl+4")), this);
   walletsTabShortcutt->setContext(Qt::ApplicationShortcut);
   connect(walletsTabShortcutt, &QShortcut::activated, [this](){ ui->tabWidget->setCurrentIndex(3);});

   auto transactionsTabShortcut = new QShortcut(QKeySequence(QString::fromStdString("Ctrl+5")), this);
   transactionsTabShortcut->setContext(Qt::ApplicationShortcut);
   connect(transactionsTabShortcut, &QShortcut::activated, [this](){ ui->tabWidget->setCurrentIndex(4);});

   auto alt_1 = new QShortcut(QKeySequence(QString::fromLatin1("Alt+1")), this);
   alt_1->setContext(Qt::ApplicationShortcut);
   connect(alt_1, &QShortcut::activated, [this]() {
         static_cast<TabWithShortcut*>(ui->tabWidget->currentWidget())->shortcutActivated(
            TabWithShortcut::ShortcutType::Alt_1);
      }
   );

   auto alt_2 = new QShortcut(QKeySequence(QString::fromLatin1("Alt+2")), this);
   alt_2->setContext(Qt::ApplicationShortcut);
   connect(alt_2, &QShortcut::activated, [this]() {
         static_cast<TabWithShortcut*>(ui->tabWidget->currentWidget())->shortcutActivated(
            TabWithShortcut::ShortcutType::Alt_2);
      }
   );

   auto alt_3 = new QShortcut(QKeySequence(QString::fromLatin1("Alt+3")), this);
   alt_3->setContext(Qt::ApplicationShortcut);
   connect(alt_3, &QShortcut::activated, [this]() {
         static_cast<TabWithShortcut*>(ui->tabWidget->currentWidget())->shortcutActivated(
            TabWithShortcut::ShortcutType::Alt_3);
      }
   );

   auto ctrl_s = new QShortcut(QKeySequence(QString::fromLatin1("Ctrl+S")), this);
   ctrl_s->setContext(Qt::ApplicationShortcut);
   connect(ctrl_s, &QShortcut::activated, [this]() {
         static_cast<TabWithShortcut*>(ui->tabWidget->currentWidget())->shortcutActivated(
            TabWithShortcut::ShortcutType::Ctrl_S);
      }
   );

   auto ctrl_p = new QShortcut(QKeySequence(QString::fromLatin1("Ctrl+P")), this);
   ctrl_p->setContext(Qt::ApplicationShortcut);
   connect(ctrl_p, &QShortcut::activated, [this]() {
         static_cast<TabWithShortcut*>(ui->tabWidget->currentWidget())->shortcutActivated(
            TabWithShortcut::ShortcutType::Ctrl_P);
      }
   );

   auto ctrl_q = new QShortcut(QKeySequence(QString::fromLatin1("Ctrl+Q")), this);
   ctrl_q->setContext(Qt::ApplicationShortcut);
   connect(ctrl_q, &QShortcut::activated, [this]() {
         static_cast<TabWithShortcut*>(ui->tabWidget->currentWidget())->shortcutActivated(
            TabWithShortcut::ShortcutType::Ctrl_Q);
      }
   );

   auto alt_s = new QShortcut(QKeySequence(QString::fromLatin1("Alt+S")), this);
   alt_s->setContext(Qt::ApplicationShortcut);
   connect(alt_s, &QShortcut::activated, [this]() {
         static_cast<TabWithShortcut*>(ui->tabWidget->currentWidget())->shortcutActivated(
            TabWithShortcut::ShortcutType::Alt_S);
      }
   );

   auto alt_b = new QShortcut(QKeySequence(QString::fromLatin1("Alt+B")), this);
   alt_b->setContext(Qt::ApplicationShortcut);
   connect(alt_b, &QShortcut::activated, [this]() {
         static_cast<TabWithShortcut*>(ui->tabWidget->currentWidget())->shortcutActivated(
            TabWithShortcut::ShortcutType::Alt_B);
      }
   );

   auto alt_p = new QShortcut(QKeySequence(QString::fromLatin1("Alt+P")), this);
   alt_p->setContext(Qt::ApplicationShortcut);
   connect(alt_p, &QShortcut::activated, [this]() {
         static_cast<TabWithShortcut*>(ui->tabWidget->currentWidget())->shortcutActivated(
            TabWithShortcut::ShortcutType::Alt_P);
      }
   );
}
