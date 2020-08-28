/*

***********************************************************************************
* Copyright (C) 2018 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "MainWindow.h"
#include <QApplication>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QGuiApplication>
#include <QIcon>
#include <QMenu>
#include <QMetaMethod>
#include <QProcess>
#include <QPushButton>
#include <QShortcut>
#include <QStringList>
#include <QSystemTrayIcon>
#include <QToolBar>
#include <QToolButton>
#include <QTreeView>
#include "ui_BSTerminalMainWindow.h"
#include "ApiAdapter.h"
#include "BSMessageBox.h"
#include "CreateTransactionDialogAdvanced.h"
#include "CreateTransactionDialogSimple.h"
#include "DialogManager.h"
#include "InfoDialogs/AboutDialog.h"
#include "InfoDialogs/StartupDialog.h"
#include "InfoDialogs/SupportDialog.h"
#include "NotificationCenter.h"
#include "Settings/ConfigDialog.h"
#include "StatusBarView.h"
#include "TabWithShortcut.h"
#include "TerminalMessage.h"
#include "UiUtils.h"

#include "terminal.pb.h"

using namespace bs::gui::qt;
using namespace BlockSettle::Terminal;

MainWindow::MainWindow(const std::shared_ptr<spdlog::logger> &logger
   , const std::shared_ptr<bs::message::QueueInterface> &queue
   , const std::shared_ptr<bs::message::User> &user)
   : QMainWindow(nullptr)
   , ui_(new Ui::BSTerminalMainWindow()), logger_(logger)
   , queue_(queue), guiUser_(user)
   , settingsUser_(std::make_shared<bs::message::UserTerminal>(bs::message::TerminalUsers::Settings))
{
   UiUtils::SetupLocale();

   ui_->setupUi(this);

   setupShortcuts();
   setupInfoWidget();

   loginButtonText_ = tr("Login");

   connect(ui_->actionQuit, &QAction::triggered, qApp, &QCoreApplication::quit);

   setupIcon();
   UiUtils::setupIconFont(this);
   notifCenter_ = std::make_shared<NotificationCenter>(logger_, nullptr, ui_.get(), sysTrayIcon_, this);

   statusBarView_ = std::make_shared<StatusBarView>(ui_->statusbar);

   setupToolbar();
   setupMenu();

   ui_->widgetTransactions->setEnabled(false);

   initChartsView();

//   ui_->tabWidget->setCurrentIndex(settings->get<int>(ApplicationSettings::GUI_main_tab));

   updateAppearance();
//   setWidgetsAuthorized(false);

   updateControlEnabledState();

   initWidgets();
}

void MainWindow::setWidgetsAuthorized(bool authorized)
{
   // Update authorized state for some widgets
   ui_->widgetPortfolio->setAuthorized(authorized);
   ui_->widgetRFQ->setAuthorized(authorized);
   ui_->widgetChart->setAuthorized(authorized);
}

void MainWindow::onGetGeometry(const QRect &mainGeom)
{
   if (mainGeom.isEmpty()) {
      return;
   }
   auto geom = mainGeom;
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
}

void MainWindow::onArmoryStateChanged(int state, unsigned int blockNum)
{
   if (statusBarView_) {
      statusBarView_->onBlockchainStateChanged(state, blockNum);
   }
}

void MainWindow::onSignerStateChanged(int state)
{
   if (statusBarView_) {
   }
}


void MainWindow::showStartupDialog(bool showLicense)
{
   StartupDialog startupDialog(showLicense, this);
//   startupDialog.init(applicationSettings_);
   int result = startupDialog.exec();

   if (showLicense && (result == QDialog::Rejected)) {
      hide();
      QTimer::singleShot(100, [] { qApp->exit(EXIT_FAILURE); });
      return;
   }

   // Need update armory settings if case user selects TestNet
   const auto &netType = startupDialog.getSelectedNetworkType();
   ApplicationSettings::EnvConfiguration envConfig = (netType == NetworkType::TestNet) ?
      ApplicationSettings::EnvConfiguration::Test : ApplicationSettings::EnvConfiguration::Production;

   SettingsMessage msg;
   auto msgArmory = msg.mutable_armory_server();
   msgArmory->set_network_type(static_cast<int>(netType));
   msgArmory->set_server_name(ARMORY_BLOCKSETTLE_NAME);

   bs::message::Envelope env{ 0, guiUser_, settingsUser_, bs::message::TimeStamp{}
      , bs::message::TimeStamp{}, msg.SerializeAsString(), true };
   queue_->pushFill(env);

   msg.Clear();
   auto msgReq = msg.mutable_put_request();
   auto msgPut = msgReq->add_responses();
   auto req = msgPut->mutable_request();
   req->set_source(SettingSource_Local);
   req->set_index(SetIdx_Environment);
   req->set_type(SettingType_Int);
   msgPut->set_i(static_cast<int>(envConfig));

   msgPut = msgReq->add_responses();
   req = msgPut->mutable_request();
   req->set_source(SettingSource_Local);
   req->set_index(SetIdx_Initialized);
   req->set_type(SettingType_Bool);
   msgPut->set_b(true);

   env = { 0, guiUser_, settingsUser_, bs::message::TimeStamp{}
      , bs::message::TimeStamp{}, msg.SerializeAsString(), true };
   queue_->pushFill(env);
}

bool MainWindow::event(QEvent *event)
{
   if (event->type() == QEvent::WindowActivate) {
      auto tabChangedSignal = QMetaMethod::fromSignal(&QTabWidget::currentChanged);
      int currentIndex = ui_->tabWidget->currentIndex();
      tabChangedSignal.invoke(ui_->tabWidget, Q_ARG(int, currentIndex));
   }
   return QMainWindow::event(event);
}

MainWindow::~MainWindow()
{
   NotificationCenter::destroyInstance();
}

void MainWindow::setupToolbar()
{
   action_send_ = new QAction(tr("Send Bitcoin"), this);
   connect(action_send_, &QAction::triggered, this, &MainWindow::onSend);

   action_generate_address_ = new QAction(tr("Generate &Address"), this);
   connect(action_generate_address_, &QAction::triggered, this, &MainWindow::onGenerateAddress);

   action_login_ = new QAction(tr("Login to BlockSettle"), this);
   connect(action_login_, &QAction::triggered, this, &MainWindow::onLoggedIn);

   action_logout_ = new QAction(tr("Logout from BlockSettle"), this);
   connect(action_logout_, &QAction::triggered, this, &MainWindow::onLoggedOut);

   setupTopRightWidget();

   action_logout_->setVisible(false);

   connect(ui_->pushButtonUser, &QPushButton::clicked, this, &MainWindow::onButtonUserClicked);

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

void MainWindow::setupTopRightWidget()
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

void MainWindow::setupIcon()
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

void MainWindow::setupInfoWidget()
{
   const bool show = true; // applicationSettings_->get<bool>(ApplicationSettings::ShowInfoWidget);
   ui_->infoWidget->setVisible(show);

/*   if (!show) {
      return;
   }*/

   connect(ui_->introductionBtn, &QPushButton::clicked, this, []() {
      QDesktopServices::openUrl(QUrl(QLatin1String("")));
   });
   connect(ui_->setUpBtn, &QPushButton::clicked, this, []() {
      QDesktopServices::openUrl(QUrl(QLatin1String("https://youtu.be/bvGNi6sBkTo")));
   });
   connect(ui_->closeBtn, &QPushButton::clicked, this, [this]() {
      ui_->infoWidget->setVisible(false);
//      applicationSettings_->set(ApplicationSettings::ShowInfoWidget, false);
   });
}

void MainWindow::updateControlEnabledState()
{
/*   if (action_send_) {
      action_send_->setEnabled(!walletsMgr_->hdWallets().empty()
         && armory_->isOnline() && signContainer_ && signContainer_->isReady());
   }*/
   // Do not allow login until wallets synced (we need to check if user has primary wallet or not).
   // Should be OK for both local and remote signer.
//   ui_->pushButtonUser->setEnabled(walletsSynched_ && loginApiKey().empty());
}

/*void MainWindow::initPortfolioView()
{
   portfolioModel_ = std::make_shared<CCPortfolioModel>(walletsMgr_, assetManager_, this);
   ui_->widgetPortfolio->init(applicationSettings_, mdProvider_, mdCallbacks_
      , portfolioModel_, signContainer_, armory_, utxoReservationMgr_, logMgr_->logger("ui"), walletsMgr_);
}

void MainWindow::initWalletsView()
{
   ui_->widgetWallets->init(logMgr_->logger("ui"), walletsMgr_, signContainer_
      , applicationSettings_, connectionManager_, assetManager_, authManager_, armory_);
   connect(ui_->widgetWallets, &WalletsWidget::newWalletCreationRequest, this, &BSTerminalMainWindow::onInitWalletDialogWasShown);
}*/

void MainWindow::initChartsView()
{
/*   ui_->widgetChart->init(applicationSettings_, mdProvider_, mdCallbacks_
      , connectionManager_, logMgr_->logger("ui"));*/
}

// Initialize widgets related to transactions.
/*void MainWindow::initTransactionsView()
{
   ui_->widgetExplorer->init(armory_, logMgr_->logger(), walletsMgr_, ccFileManager_, authManager_);
   ui_->widgetTransactions->init(walletsMgr_, armory_, utxoReservationMgr_, signContainer_, applicationSettings_
                                , logMgr_->logger("ui"));
   ui_->widgetTransactions->setEnabled(true);

   ui_->widgetTransactions->SetTransactionsModel(transactionsModel_);
   ui_->widgetPortfolio->SetTransactionsModel(transactionsModel_);
}*/

void MainWindow::onReactivate()
{
   show();
}

void MainWindow::raiseWindow()
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

void MainWindow::updateAppearance()
{
/*   if (!applicationSettings_->get<bool>(ApplicationSettings::closeToTray) && isHidden()) {
      setWindowState(windowState() & ~Qt::WindowMinimized);
      show();
      raise();
      activateWindow();
   }*/

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

void MainWindow::onGenerateAddress()
{
/*   if (walletsMgr_->hdWallets().empty()) {
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
   */
}

void MainWindow::onSend()
{
   std::string selectedWalletId;

   if (ui_->tabWidget->currentWidget() == ui_->widgetWallets) {
      auto wallet = ui_->widgetWallets->getSelectedHdWallet();
      if (!wallet) {
//         wallet = walletsMgr_->getPrimaryWallet();
      }
      if (wallet) {
//         selectedWalletId = wallet->walletId();
      }
   }


   std::shared_ptr<CreateTransactionDialog> dlg;

/*   if ((QGuiApplication::keyboardModifiers() & Qt::ShiftModifier)
       || applicationSettings_->get<bool>(ApplicationSettings::AdvancedTxDialogByDefault)) {
      dlg = std::make_shared<CreateTransactionDialogAdvanced>(armory_, walletsMgr_, utxoReservationMgr_
         , signContainer_, true, logMgr_->logger("ui"), applicationSettings_, nullptr, bs::UtxoReservationToken{}, this );
   } else {
      dlg = std::make_shared<CreateTransactionDialogSimple>(armory_, walletsMgr_, utxoReservationMgr_, signContainer_
            , logMgr_->logger("ui"), applicationSettings_, this);
   }*/

   if (!selectedWalletId.empty()) {
      dlg->SelectWallet(selectedWalletId, UiUtils::WalletsTypes::None);
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

void MainWindow::setupMenu()
{
   // menu role erquired for OSX only, to place it to first menu item
   action_login_->setMenuRole(QAction::ApplicationSpecificRole);
   action_logout_->setMenuRole(QAction::ApplicationSpecificRole);


   ui_->menuFile->insertAction(ui_->actionSettings, action_login_);
   ui_->menuFile->insertAction(ui_->actionSettings, action_logout_);

   ui_->menuFile->insertSeparator(action_login_);
   ui_->menuFile->insertSeparator(ui_->actionSettings);

/*   AboutDialog *aboutDlg = new AboutDialog(applicationSettings_->get<QString>(ApplicationSettings::ChangeLog_Base_Url), this);
   auto aboutDlgCb = [aboutDlg] (int tab) {
      return [aboutDlg, tab]() {
         aboutDlg->setTab(tab);
         aboutDlg->show();
      };
   };*/

   SupportDialog *supportDlg = new SupportDialog(this);
   auto supportDlgCb = [supportDlg] (int tab, QString title) {
      return [supportDlg, tab, title]() {
         supportDlg->setTab(tab);
         supportDlg->setWindowTitle(title);
         supportDlg->show();
      };
   };

   connect(ui_->actionCreateNewWallet, &QAction::triggered, this, [ww = ui_->widgetWallets]{ ww->onNewWallet(); });
//   connect(ui_->actionAuthenticationAddresses, &QAction::triggered, this, &MainWindow::openAuthManagerDialog);
   connect(ui_->actionSettings, &QAction::triggered, this, [=]() { openConfigDialog(); });
//   connect(ui_->actionAccountInformation, &QAction::triggered, this, &MainWindow::openAccountInfoDialog);
//   connect(ui_->actionEnterColorCoinToken, &QAction::triggered, this, &MainWindow::openCCTokenDialog);
/*   connect(ui_->actionAbout, &QAction::triggered, aboutDlgCb(0));
   connect(ui_->actionVersion, &QAction::triggered, aboutDlgCb(3));*/
   connect(ui_->actionGuides, &QAction::triggered, supportDlgCb(0, QObject::tr("Guides")));
   connect(ui_->actionVideoTutorials, &QAction::triggered, supportDlgCb(1, QObject::tr("Video Tutorials")));
   connect(ui_->actionContact, &QAction::triggered, supportDlgCb(2, QObject::tr("Support")));

   onUserLoggedOut();

#ifndef Q_OS_MAC
   ui_->horizontalFrame->hide();
   ui_->menubar->setCornerWidget(ui_->loginGroupWidget);
#endif

#ifndef PRODUCTION_BUILD
/*   auto envType = static_cast<ApplicationSettings::EnvConfiguration>(applicationSettings_->get(ApplicationSettings::envConfiguration).toInt());
   bool isProd = envType == ApplicationSettings::EnvConfiguration::Production;
   ui_->prodEnvSettings->setEnabled(!isProd);
   ui_->testEnvSettings->setEnabled(isProd);*/
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

void MainWindow::openConfigDialog(bool showInNetworkPage)
{
/*   ConfigDialog configDialog(applicationSettings_, armoryServersProvider_, signersProvider_, signContainer_, this);
   connect(&configDialog, &ConfigDialog::reconnectArmory, this, &BSTerminalMainWindow::onArmoryNeedsReconnect);

   if (showInNetworkPage) {
      configDialog.popupNetworkSettings();
   }
   configDialog.exec();*/

   updateAppearance();
}

void MainWindow::onLoggedIn()
{
//   onNetworkSettingsRequired(NetworkSettingsClient::Login);
}

/*void MainWindow::onLoginProceed(const NetworkSettings &networkSettings)
{
   auto envType = static_cast<ApplicationSettings::EnvConfiguration>(applicationSettings_->get(ApplicationSettings::envConfiguration).toInt());

#ifdef PRODUCTION_BUILD
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
#endif

   if (walletsSynched_ && !walletsMgr_->getPrimaryWallet()) {
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

   networkSettingsReceived(networkSettings, NetworkSettingsClient::MarketData);

   activateClient(bsClient, *loginDialog.result(), loginDialog.email().toStdString());
}*/

void MainWindow::onLoggedOut()
{
   ui_->widgetWallets->setUsername(QString());
/*   if (chatClientServicePtr_) {
      chatClientServicePtr_->LogoutFromServer();
   }*/
   ui_->widgetChart->disconnect();

/*   if (celerConnection_->IsConnected()) {
      celerConnection_->CloseConnection();
   }*/

//   mdProvider_->UnsubscribeFromMD();

   setLoginButtonText(loginButtonText_);

   setWidgetsAuthorized(false);

//   bsClient_.reset();
}

void MainWindow::onUserLoggedIn()
{
   ui_->actionAccountInformation->setEnabled(true);
/*   ui_->actionAuthenticationAddresses->setEnabled(celerConnection_->celerUserType()
      != BaseCelerClient::CelerUserType::Market);*/
   ui_->actionOneTimePassword->setEnabled(true);
   ui_->actionEnterColorCoinToken->setEnabled(true);

   ui_->actionDeposits->setEnabled(true);
   ui_->actionWithdrawalRequest->setEnabled(true);
   ui_->actionLinkAdditionalBankAccount->setEnabled(true);

//   ccFileManager_->ConnectToCelerClient(celerConnection_);
//   ui_->widgetRFQ->onUserConnected(userType_);
//   ui_->widgetRFQReply->onUserConnected(userType_);

//   const auto userId = BinaryData::CreateFromHex(celerConnection_->userId());
/*   const auto &deferredDialog = [this, userId] {
      walletsMgr_->setUserId(userId);
      promoteToPrimaryIfNeeded();
   };
   addDeferredDialog(deferredDialog);*/

   setLoginButtonText(currentUserLogin_);
}

void MainWindow::onUserLoggedOut()
{
   ui_->actionAccountInformation->setEnabled(false);
   ui_->actionAuthenticationAddresses->setEnabled(false);
   ui_->actionEnterColorCoinToken->setEnabled(false);
   ui_->actionOneTimePassword->setEnabled(false);

   ui_->actionDeposits->setEnabled(false);
   ui_->actionWithdrawalRequest->setEnabled(false);
   ui_->actionLinkAdditionalBankAccount->setEnabled(false);

/*   if (walletsMgr_) {
      walletsMgr_->setUserId(BinaryData{});
   }
   if (authManager_) {
      authManager_->OnDisconnectedFromCeler();
   }*/

   setLoginButtonText(loginButtonText_);
}

/*void BSTerminalMainWindow::onCelerConnected()
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
}*/

void MainWindow::showRunInBackgroundMessage()
{
   sysTrayIcon_->showMessage(tr("BlockSettle is running")
      , tr("BlockSettle Terminal is running in the backgroud. Click the tray icon to open the main window.")
      , QIcon(QLatin1String(":/resources/login-logo.png")));
}

void MainWindow::closeEvent(QCloseEvent* event)
{
/*   if (applicationSettings_->get<bool>(ApplicationSettings::closeToTray)) {
      hide();
      event->ignore();
   }
   else {
      if (chatClientServicePtr_) {
         chatClientServicePtr_->LogoutFromServer();
      }
      */
      QMainWindow::closeEvent(event);
      QApplication::exit();
//   }
}

void MainWindow::changeEvent(QEvent* e)
{
   switch (e->type()) {
      case QEvent::WindowStateChange:
         if (this->windowState() & Qt::WindowMinimized) {
/*            if (applicationSettings_->get<bool>(ApplicationSettings::minimizeToTray))
            {
               QTimer::singleShot(0, this, &QMainWindow::hide);
            }*/
         }
         break;
      default:
         break;
   }
   QMainWindow::changeEvent(e);
}

void MainWindow::setLoginButtonText(const QString &text)
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

void MainWindow::setupShortcuts()
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

void MainWindow::onButtonUserClicked() {
   if (ui_->pushButtonUser->text() == loginButtonText_) {
      onLoggedIn();
   } else {
      if (BSMessageBox(BSMessageBox::question, tr("User Logout"), tr("You are about to logout")
         , tr("Do you want to continue?")).exec() == QDialog::Accepted)
      onLoggedOut();
   }
}

/*void MainWindow::onTabWidgetCurrentChanged(const int &index)
{
   const int chatIndex = ui_->tabWidget->indexOf(ui_->widgetChat);
   const bool isChatTab = index == chatIndex;
   //ui_->widgetChat->updateChat(isChatTab);
}*/

void MainWindow::onSignerVisibleChanged()
{
   processDeferredDialogs();
}

void MainWindow::initWidgets()
{
//   InitWalletsView();
//   InitPortfolioView();

//   ui_->widgetRFQ->initWidgets(mdProvider_, mdCallbacks_, applicationSettings_);

#if 0
   const auto aqScriptRunner = new AQScriptRunner(quoteProvider, signContainer_
      , mdCallbacks_, assetManager_, logger);
   if (!applicationSettings_->get<std::string>(ApplicationSettings::ExtConnName).empty()
      && !applicationSettings_->get<std::string>(ApplicationSettings::ExtConnHost).empty()
      && !applicationSettings_->get<std::string>(ApplicationSettings::ExtConnPort).empty()
      /*&& !applicationSettings_->get<std::string>(ApplicationSettings::ExtConnPubKey).empty()*/) {
      ExtConnections extConns;
/*      bs::network::BIP15xParams params;
      params.ephemeralPeers = true;
      params.cookie = bs::network::BIP15xCookie::ReadServer;
      params.serverPublicKey = BinaryData::CreateFromHex(applicationSettings_->get<std::string>(
         ApplicationSettings::ExtConnPubKey));
      const auto &bip15xTransport = std::make_shared<bs::network::TransportBIP15xClient>(logger, params);
      bip15xTransport->setKeyCb(cbApproveExtConn_);*/

      logger->debug("Setting up ext connection");
      auto connection = std::make_shared<WsDataConnection>(logger, WsDataConnectionParams{ });
      //TODO: BIP15x will be superceded with SSL with certificate checking on both ends
//      auto wsConnection = std::make_unique<WsDataConnection>(logger, WsDataConnectionParams{});
//      auto connection = std::make_shared<Bip15xDataConnection>(logger, std::move(wsConnection), bip15xTransport);
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
#endif //0

   auto dialogManager = std::make_shared<DialogManager>(this);

/*   ui_->widgetRFQ->init(logger, celerConnection_, authManager_, quoteProvider
      , assetManager_, dialogManager, signContainer_, armory_, autoSignRFQProvider_
      , utxoReservationMgr_, orderListModel_.get());
   ui_->widgetRFQReply->init(logger, celerConnection_, authManager_
      , quoteProvider, mdCallbacks_, assetManager_, applicationSettings_, dialogManager
      , signContainer_, armory_, connectionManager_, autoSignQuoteProvider_
      , utxoReservationMgr_, orderListModel_.get());

   connect(ui_->widgetRFQ, &RFQRequestWidget::requestPrimaryWalletCreation, this
      , &BSTerminalMainWindow::onCreatePrimaryWalletRequest);
   connect(ui_->widgetRFQReply, &RFQReplyWidget::requestPrimaryWalletCreation, this
      , &BSTerminalMainWindow::onCreatePrimaryWalletRequest);*/

   connect(ui_->tabWidget, &QTabWidget::tabBarClicked, this,
      [/*requestRFQ = QPointer<RFQRequestWidget>(ui_->widgetRFQ)
         , replyRFQ = QPointer<RFQReplyWidget>(ui_->widgetRFQReply)
         ,*/ tabWidget = QPointer<QTabWidget>(ui_->tabWidget)] (int index)
   {
      if (!tabWidget) {
         return;
      }
/*      if (requestRFQ && requestRFQ == tabWidget->widget(index)) {
         requestRFQ->forceCheckCondition();
      }
      if (replyRFQ && replyRFQ == tabWidget->widget(index)) {
         replyRFQ->forceCheckCondition();
      }*/
   });
}

void MainWindow::promptSwitchEnv(bool prod)
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

void MainWindow::switchToTestEnv()
{
/*   applicationSettings_->set(ApplicationSettings::envConfiguration
      , static_cast<int>(ApplicationSettings::EnvConfiguration::Test));
   armoryServersProvider_->setupServer(armoryServersProvider_->getIndexOfTestNetServer());*/
}

void MainWindow::switchToProdEnv()
{
/*   applicationSettings_->set(ApplicationSettings::envConfiguration
      , static_cast<int>(ApplicationSettings::EnvConfiguration::Production));
   armoryServersProvider_->setupServer(armoryServersProvider_->getIndexOfMainNetServer());*/
}

void MainWindow::restartTerminal()
{
//   lockFile_.unlock();
   QProcess::startDetached(qApp->arguments()[0], qApp->arguments());
   qApp->quit();
}

void MainWindow::processDeferredDialogs()
{
   if(deferredDialogRunning_) {
      return;
   }
/*   if (signContainer_ && signContainer_->isLocal() && signContainer_->isWindowVisible()) {
      return;
   }*/

   deferredDialogRunning_ = true;
   while (!deferredDialogs_.empty()) {
      deferredDialogs_.front()(); // run stored lambda
      deferredDialogs_.pop();
   }
   deferredDialogRunning_ = false;
}

void MainWindow::addDeferredDialog(const std::function<void(void)> &deferredDialog)
{
   // multi thread scope, it's safe to call this function from different threads
   QMetaObject::invokeMethod(this, [this, deferredDialog] {
      // single thread scope (main thread), it's safe to push to deferredDialogs_
      // and check deferredDialogRunning_ variable
      deferredDialogs_.push(deferredDialog);
      processDeferredDialogs();
   }, Qt::QueuedConnection);
}
