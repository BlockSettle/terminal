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
#include "ApplicationSettings.h"
#include "AuthAddressDialog.h"
#include "BSMessageBox.h"
#include "CreateTransactionDialogAdvanced.h"
#include "CreateTransactionDialogSimple.h"
#include "DialogManager.h"
#include "InfoDialogs/AboutDialog.h"
#include "InfoDialogs/StartupDialog.h"
#include "InfoDialogs/SupportDialog.h"
#include "LoginWindow.h"
#include "NotificationCenter.h"
#include "OrderListModel.h"
#include "Settings/ConfigDialog.h"
#include "StatusBarView.h"
#include "TabWithShortcut.h"
#include "TerminalMessage.h"
#include "TransactionsViewModel.h"
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
   notifCenter_ = std::make_shared<NotificationCenter>(logger_, ui_.get(), sysTrayIcon_, this);

   statusBarView_ = std::make_shared<StatusBarView>(ui_->statusbar);

   setupToolbar();
   setupMenu();

   initChartsView();

   updateAppearance();
   setWidgetsAuthorized(false);

   initWidgets();

   ui_->widgetTransactions->setEnabled(false);
   actSend_->setEnabled(false);
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
   /*
   const auto screen = qApp->screens()[screenNo];
   const float pixelRatio = screen->devicePixelRatio();
   if (pixelRatio > 1.0) {
      //FIXME: re-check on hi-res screen
   }*/
#else
   if (QApplication::desktop()->screenNumber(this) == -1) {
      auto currentScreenRect = QApplication::desktop()->screenGeometry(QCursor::pos());
      // Do not delete 0.9 multiplier, since in some system window size is applying without system native toolbar
      geom.setWidth(std::min(geom.width(), static_cast<int>(currentScreenRect.width() * 0.9)));
      geom.setHeight(std::min(geom.height(), static_cast<int>(currentScreenRect.height() * 0.9)));
      geom.moveCenter(currentScreenRect.center());
   }
#endif   // not Windows
   QTimer::singleShot(10, [this, geom] {
      setGeometry(geom);
   });
}

void MainWindow::onSetting(int setting, const QVariant &value)
{
   switch (static_cast<ApplicationSettings::Setting>(setting)) {
   case ApplicationSettings::GUI_main_tab:
      ui_->tabWidget->setCurrentIndex(value.toInt());
      break;
   case ApplicationSettings::ShowInfoWidget:
      ui_->infoWidget->setVisible(value.toBool());
      break;
   case ApplicationSettings::AdvancedTxDialogByDefault:
      advTxDlgByDefault_ = value.toBool();
      break;
   case ApplicationSettings::closeToTray:
      closeToTray_ = value.toBool();
      updateAppearance();
      break;
   case ApplicationSettings::envConfiguration: {
      const auto& newEnvCfg = static_cast<ApplicationSettings::EnvConfiguration>(value.toInt());
      if (envConfig_ != newEnvCfg) {
         envConfig_ = newEnvCfg;
         //TODO: maybe initiate relog and Celer/proxy reconnect
      }
   }
      break;
   case ApplicationSettings::rememberLoginUserName:
      if (loginDlg_) {
         loginDlg_->setRememberLogin(value.toBool());
      }
      break;
   case ApplicationSettings::celerUsername:
      if (loginDlg_) {
         loginDlg_->setLogin(value.toString());
      }
      break;
   default: break;
   }

   if (cfgDlg_) {
      cfgDlg_->onSetting(setting, value);
   }
}

void bs::gui::qt::MainWindow::onSettingsState(const ApplicationSettings::State& state)
{
   if (cfgDlg_) {
      cfgDlg_->onSettingsState(state);
   }
}

void MainWindow::onArmoryStateChanged(int state, unsigned int blockNum)
{
   topBlock_ = blockNum;
   statusBarView_->onBlockchainStateChanged(state, blockNum);
   ui_->widgetExplorer->onNewBlock(blockNum);
}

void MainWindow::onNewBlock(int state, unsigned int blockNum)
{
   topBlock_ = blockNum;
   statusBarView_->onBlockchainStateChanged(state, blockNum);

   if (txModel_) {
      txModel_->onNewBlock(blockNum);
   }
   ui_->widgetWallets->onNewBlock(blockNum);
   ui_->widgetExplorer->onNewBlock(blockNum);
}

void MainWindow::onWalletsReady()
{
   logger_->debug("[{}]", __func__);
   ui_->widgetTransactions->setEnabled(true);
   actSend_->setEnabled(true);
   emit needLedgerEntries({});
}

void MainWindow::onSignerStateChanged(int state, const std::string &details)
{
   statusBarView_->onSignerStatusChanged(static_cast<SignContainer::ConnectionError>(state)
      , QString::fromStdString(details));
}

void MainWindow::onHDWallet(const bs::sync::WalletInfo &wi)
{
   ui_->widgetWallets->onHDWallet(wi);
}

void MainWindow::onHDWalletDetails(const bs::sync::HDWalletData &hdWallet)
{
   ui_->widgetWallets->onHDWalletDetails(hdWallet);
   ui_->widgetRFQ->onHDWallet(hdWallet);
   ui_->widgetRFQReply->onHDWallet(hdWallet);
}

void MainWindow::onWalletsList(const std::string &id, const std::vector<bs::sync::HDWalletData>& wallets)
{
   if (txDlg_) {
      txDlg_->onWalletsList(id, wallets);
   }
}

void bs::gui::qt::MainWindow::onWalletData(const std::string& walletId
   , const bs::sync::WalletData& wd)
{
   ui_->widgetRFQ->onWalletData(walletId, wd);
}

void MainWindow::onAddresses(const std::vector<bs::sync::Address> &addrs)
{
   if (txDlg_) {
      txDlg_->onAddresses(addrs);
   }
   else {
      ui_->widgetWallets->onAddresses(addrs);
   }
}

void MainWindow::onAddressComments(const std::string &walletId
   , const std::map<bs::Address, std::string> &comments)
{
   if (txDlg_) {
      txDlg_->onAddressComments(walletId, comments);
   }
   else {
      ui_->widgetWallets->onAddressComments(walletId, comments);
   }
}

void MainWindow::onWalletBalance(const bs::sync::WalletBalanceData &wbd)
{
   if (txDlg_) {
      txDlg_->onAddressBalances(wbd.id, wbd.addrBalances);
   }
   ui_->widgetWallets->onWalletBalance(wbd);
   ui_->widgetRFQ->onWalletBalance(wbd);
   ui_->widgetRFQReply->onWalletBalance(wbd);
   statusBarView_->onXbtBalance(wbd);
}

void MainWindow::onLedgerEntries(const std::string &filter, uint32_t totalPages
   , uint32_t curPage, uint32_t curBlock, const std::vector<bs::TXEntry> &entries)
{
   if (filter.empty()) {
      txModel_->onLedgerEntries(filter, totalPages, curPage, curBlock, entries);
   }
   else {
      ui_->widgetWallets->onLedgerEntries(filter, totalPages, curPage, curBlock, entries);
   }
}

void MainWindow::onTXDetails(const std::vector<bs::sync::TXWalletDetails> &txDet)
{
   txModel_->onTXDetails(txDet);
   ui_->widgetWallets->onTXDetails(txDet);
   ui_->widgetExplorer->onTXDetails(txDet);
}

void bs::gui::qt::MainWindow::onNewZCs(const std::vector<bs::sync::TXWalletDetails>& txDet)
{
   QStringList lines;
   for (const auto& tx : txDet) {
      lines << tr("TX: %1 %2 %3").arg(tr(bs::sync::Transaction::toString(tx.direction)))
         .arg(QString::fromStdString(tx.amount))
         .arg(QString::fromStdString(tx.walletSymbol));
      if (!tx.walletName.empty()) {
         lines << tr("Wallet: %1").arg(QString::fromStdString(tx.walletName));
      }

      QString mainAddress, multipleAddresses = tr("%1 output addresses");
      switch (tx.outAddresses.size()) {
      case 0:
         switch (tx.outputAddresses.size()) {
         case 1:
            mainAddress = QString::fromStdString(tx.outputAddresses.at(0).address.display());
            break;
         default:
            mainAddress = multipleAddresses.arg(tx.outputAddresses.size());
            break;
         }
         break;
      case 1:
         mainAddress = QString::fromStdString(tx.outAddresses.at(0).display());
         break;
      default:
         mainAddress = multipleAddresses.arg(tx.outAddresses.size());
         break;
      }
      lines << mainAddress << QString();
   }
   const auto& title = tr("New blockchain transaction");
   notifCenter_->enqueue(bs::ui::NotifyType::BlockchainTX, { title, lines.join(tr("\n")) });
}

void bs::gui::qt::MainWindow::onZCsInvalidated(const std::vector<BinaryData>& txHashes)
{
   if (txModel_) {
      txModel_->onZCsInvalidated(txHashes);
   }
}

void MainWindow::onAddressHistory(const bs::Address& addr, uint32_t curBlock, const std::vector<bs::TXEntry>& entries)
{
   ui_->widgetExplorer->onAddressHistory(addr, curBlock, entries);
}

void MainWindow::onFeeLevels(const std::map<unsigned int, float>& feeLevels)
{
   if (txDlg_) {
      txDlg_->onFeeLevels(feeLevels);
   }
}

void bs::gui::qt::MainWindow::onUTXOs(const std::string& id
   , const std::string& walletId, const std::vector<UTXO>& utxos)
{
   if (txDlg_) {
      txDlg_->onUTXOs(id, walletId, utxos);
   }
}

void bs::gui::qt::MainWindow::onSignedTX(const std::string& id, BinaryData signedTX
   , bs::error::ErrorCode result)
{
   if (txDlg_) {
      txDlg_->onSignedTX(id, signedTX, result);
   }
}

void bs::gui::qt::MainWindow::onArmoryServers(const QList<ArmoryServer>& servers, int idxCur, int idxConn)
{
   if (cfgDlg_) {
      cfgDlg_->onArmoryServers(servers, idxCur, idxConn);
   }
}

void bs::gui::qt::MainWindow::onSignerSettings(const QList<SignerHost>& signers
   , const std::string& ownKey, int idxCur)
{
   if (cfgDlg_) {
      cfgDlg_->onSignerSettings(signers, ownKey, idxCur);
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
   actSend_ = new QAction(tr("Send Bitcoin"), this);
   connect(actSend_, &QAction::triggered, this, &MainWindow::onSend);

   actNewAddress_ = new QAction(tr("Generate &Address"), this);
   connect(actNewAddress_, &QAction::triggered, this, &MainWindow::onGenerateAddress);

   actLogin_ = new QAction(tr("Login to BlockSettle"), this);
   connect(actLogin_, &QAction::triggered, this, &MainWindow::onLoginInitiated);

   actLogout_ = new QAction(tr("Logout from BlockSettle"), this);
   connect(actLogout_, &QAction::triggered, this, &MainWindow::onLogoutInitiated);

   setupTopRightWidget();

   actLogout_->setVisible(false);

   connect(ui_->pushButtonUser, &QPushButton::clicked, this, &MainWindow::onButtonUserClicked);

   QMenu* trayMenu = new QMenu(this);
   QAction* trayShowAction = trayMenu->addAction(tr("&Open Terminal"));
   connect(trayShowAction, &QAction::triggered, this, &QMainWindow::show);
   trayMenu->addSeparator();

   trayMenu->addAction(actSend_);
   trayMenu->addAction(actNewAddress_);
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

   toolBar->addAction(actSend_);
   toolBar->addAction(actNewAddress_);

   for (int i = 0; i < toolBar->children().size(); ++i) {
      auto *toolButton = qobject_cast<QToolButton*>(toolBar->children().at(i));
      if (toolButton && (toolButton->defaultAction() == actSend_
         || toolButton->defaultAction() == actNewAddress_)) {
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
   connect(ui_->introductionBtn, &QPushButton::clicked, this, []() {
      QDesktopServices::openUrl(QUrl(QLatin1String("https://www.youtube.com/watch?v=mUqKq9GKjmI")));
   });
   connect(ui_->tutorialsButton, &QPushButton::clicked, this, []() {
      QDesktopServices::openUrl(QUrl(QLatin1String("https://blocksettle.com/tutorials")));
   });
   connect(ui_->closeBtn, &QPushButton::clicked, this, [this]() {
      ui_->infoWidget->setVisible(false);
      emit putSetting(ApplicationSettings::ShowInfoWidget, false);
   });
}

void MainWindow::initChartsView()
{
/*   ui_->widgetChart->init(applicationSettings_, mdProvider_, mdCallbacks_
      , connectionManager_, logMgr_->logger("ui"));*/
}

// Initialize widgets related to transactions.
void MainWindow::initTransactionsView()
{
   txModel_ = std::make_shared<TransactionsViewModel>(logger_, this);
   connect(txModel_.get(), &TransactionsViewModel::needTXDetails, this
      , &MainWindow::needTXDetails);

   ui_->widgetExplorer->init(logger_);
   ui_->widgetTransactions->init(logger_, txModel_);
   ui_->widgetTransactions->setEnabled(true);

   ui_->widgetPortfolio->SetTransactionsModel(txModel_);
}

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
   if (!closeToTray_ && isHidden()) {
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
      if (!wallet.ids.empty()) {
         selectedWalletId = *wallet.ids.cbegin();
      }
   }

   if ((QGuiApplication::keyboardModifiers() & Qt::ShiftModifier) || advTxDlgByDefault_) {
      const bool loadFees = true;
      txDlg_ = new CreateTransactionDialogAdvanced(loadFees, topBlock_, logger_
         , nullptr, bs::UtxoReservationToken{}, this );
   } else {
      txDlg_ = new CreateTransactionDialogSimple(topBlock_, logger_, this);
   }
   connect(txDlg_, &QDialog::finished, [this](int) {
      txDlg_->deleteLater();
      txDlg_ = nullptr;
   });
   connect(txDlg_, &CreateTransactionDialog::needWalletsList, this, &MainWindow::needWalletsList);
   connect(txDlg_, &CreateTransactionDialog::needFeeLevels, this, &MainWindow::needFeeLevels);
   connect(txDlg_, &CreateTransactionDialog::needUTXOs, this, &MainWindow::needUTXOs);
   connect(txDlg_, &CreateTransactionDialog::needExtAddresses, this, &MainWindow::needExtAddresses);
   connect(txDlg_, &CreateTransactionDialog::needIntAddresses, this, &MainWindow::needIntAddresses);
   connect(txDlg_, &CreateTransactionDialog::needUsedAddresses, this, &MainWindow::needUsedAddresses);
   connect(txDlg_, &CreateTransactionDialog::needAddrComments, this, &MainWindow::needAddrComments);
   connect(txDlg_, &CreateTransactionDialog::needWalletBalances, this, &MainWindow::needWalletBalances);
   connect(txDlg_, &CreateTransactionDialog::needSignTX, this, &MainWindow::needSignTX);
   connect(txDlg_, &CreateTransactionDialog::needBroadcastZC, this, &MainWindow::needBroadcastZC);
   connect(txDlg_, &CreateTransactionDialog::needSetTxComment, this, &MainWindow::needSetTxComment);

   txDlg_->initUI();
   if (!selectedWalletId.empty()) {
      txDlg_->SelectWallet(selectedWalletId, UiUtils::WalletsTypes::None);
   }
   txDlg_->exec();
}

void MainWindow::setupMenu()
{
   // menu role erquired for OSX only, to place it to first menu item
   actLogin_->setMenuRole(QAction::ApplicationSpecificRole);
   actLogout_->setMenuRole(QAction::ApplicationSpecificRole);


   ui_->menuFile->insertAction(ui_->actionSettings, actLogin_);
   ui_->menuFile->insertAction(ui_->actionSettings, actLogout_);

   ui_->menuFile->insertSeparator(actLogin_);
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
   connect(ui_->actionAuthenticationAddresses, &QAction::triggered, this, &MainWindow::openAuthManagerDialog);
   connect(ui_->actionSettings, &QAction::triggered, this, [=]() { openConfigDialog(); });
//   connect(ui_->actionAccountInformation, &QAction::triggered, this, &MainWindow::openAccountInfoDialog);
//   connect(ui_->actionEnterColorCoinToken, &QAction::triggered, this, &MainWindow::openCCTokenDialog);
/*   connect(ui_->actionAbout, &QAction::triggered, aboutDlgCb(0));
   connect(ui_->actionVersion, &QAction::triggered, aboutDlgCb(3));*/
   connect(ui_->actionGuides, &QAction::triggered, supportDlgCb(0, QObject::tr("Guides")));
   connect(ui_->actionVideoTutorials, &QAction::triggered, supportDlgCb(1, QObject::tr("Video Tutorials")));
   connect(ui_->actionContact, &QAction::triggered, supportDlgCb(2, QObject::tr("Support")));

   onMatchingLogout();

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

void bs::gui::qt::MainWindow::openAuthManagerDialog()
{
   if (authAddrDlg_) {
      authAddrDlg_->exec();
   }
}

void MainWindow::openConfigDialog(bool showInNetworkPage)
{
   cfgDlg_ = new ConfigDialog(this);
   connect(cfgDlg_, &QDialog::finished, [this](int) {
      cfgDlg_->deleteLater();
      cfgDlg_ = nullptr;
   });
   connect(cfgDlg_, &ConfigDialog::reconnectArmory, this, &MainWindow::needArmoryReconnect);
   connect(cfgDlg_, &ConfigDialog::putSetting, this, &MainWindow::putSetting);
   connect(cfgDlg_, &ConfigDialog::resetSettings, this, &MainWindow::resetSettings);
   connect(cfgDlg_, &ConfigDialog::resetSettingsToState, this, &MainWindow::resetSettingsToState);
   connect(cfgDlg_, &ConfigDialog::resetSettingsToState, this, &MainWindow::resetSettingsToState);
   connect(cfgDlg_, &ConfigDialog::setArmoryServer, this, &MainWindow::setArmoryServer);
   connect(cfgDlg_, &ConfigDialog::addArmoryServer, this, &MainWindow::addArmoryServer);
   connect(cfgDlg_, &ConfigDialog::delArmoryServer, this, &MainWindow::delArmoryServer);
   connect(cfgDlg_, &ConfigDialog::updArmoryServer, this, &MainWindow::updArmoryServer);
   connect(cfgDlg_, &ConfigDialog::setSigner, this, &MainWindow::setSigner);

   emit needSettingsState();
   emit needArmoryServers();
   emit needSigners();

   if (showInNetworkPage) {
      cfgDlg_->popupNetworkSettings();
   }
   cfgDlg_->exec();
}

void MainWindow::onLoginInitiated()
{
   if (!actLogin_->isEnabled()) {
      return;
   }
   //TODO: prompt for primary wallet creation if needed
   emit needOpenBsConnection();
   loginDlg_ = new LoginWindow(logger_, envConfig_, this);
   emit getSettings({ ApplicationSettings::rememberLoginUserName, ApplicationSettings::celerUsername });
   connect(loginDlg_, &QDialog::finished, [this] {
      loginDlg_->deleteLater();
      loginDlg_ = nullptr;
   });
   connect(loginDlg_, &LoginWindow::putSetting, this, &MainWindow::putSetting);
   connect(loginDlg_, &LoginWindow::needStartLogin, this, &MainWindow::needStartLogin);
   connect(loginDlg_, &LoginWindow::needCancelLogin, this, &MainWindow::needCancelLogin);

   loginDlg_->exec();
}

void bs::gui::qt::MainWindow::onLoginStarted(const std::string& login, bool success, const std::string& errMsg)
{
   if (loginDlg_) {
      loginDlg_->onLoginStarted(login, success, errMsg);
   }
}

void MainWindow::onLoggedIn(const BsClientLoginResult& result)
{
   if (loginDlg_) {
      loginDlg_->onLoggedIn(result);
   }
   if (result.status != AutheIDClient::ErrorType::NoError) {
      onLoggedOut();
      return;
   }
   bool isRegistered = (result.userType == bs::network::UserType::Market
      || result.userType == bs::network::UserType::Trading
      || result.userType == bs::network::UserType::Dealing);

   if (!isRegistered && envConfig_ == ApplicationSettings::EnvConfiguration::Test) {
      auto createTestAccountUrl = tr("async retrieval of GetAccount_UrlTest");
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

   if (!isRegistered && envConfig_ == ApplicationSettings::EnvConfiguration::Production) {
      auto createAccountUrl = tr("async retrieval of GetAccount_UrlProd");
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

   activateClient(result);
}

void MainWindow::activateClient(const BsClientLoginResult& result)
{
   currentUserLogin_ = QString::fromStdString(result.login);

   auto tradeSettings = std::make_shared<bs::TradeSettings>(result.tradeSettings);
   emit putSetting(ApplicationSettings::SubmittedAddressXbtLimit
      , static_cast<quint64>(tradeSettings->xbtTier1Limit));

   setLoginButtonText(currentUserLogin_);
   setWidgetsAuthorized(true);

   ui_->widgetRFQ->onTradeSettings(tradeSettings);

   ui_->widgetWallets->setUsername(currentUserLogin_);
   actLogout_->setVisible(false);
   actLogin_->setEnabled(false);

   // Market data, charts and chat should be available for all Auth eID logins
   emit needMdConnection(envConfig_);

   accountEnabled_ = result.enabled;
   onAccountTypeChanged(result.userType, result.enabled);

   if (!authAddrDlg_) {
      authAddrDlg_ = new AuthAddressDialog(logger_, this);
      connect(authAddrDlg_, &AuthAddressDialog::putSetting, this, &MainWindow::putSetting);
      connect(authAddrDlg_, &AuthAddressDialog::needNewAuthAddress, this, &MainWindow::needNewAuthAddress);
      connect(authAddrDlg_, &AuthAddressDialog::needSubmitAuthAddress, this, &MainWindow::needSubmitAuthAddress);
   }
}

void MainWindow::onAccountTypeChanged(bs::network::UserType userType, bool enabled)
{
//   userType_ = userType;
   if ((accountEnabled_ != enabled) && (userType != bs::network::UserType::Chat)) {
      notifCenter_->enqueue(enabled ? bs::ui::NotifyType::AccountEnabled
         : bs::ui::NotifyType::AccountDisabled, {});
   }
   ui_->widgetChat->setUserType(enabled ? userType : bs::network::UserType::Chat);
}

void bs::gui::qt::MainWindow::onMatchingLogin(const std::string& mtchLogin
   , BaseCelerClient::CelerUserType userType, const std::string& userId)
{
   ui_->actionAccountInformation->setEnabled(true);
   ui_->actionAuthenticationAddresses->setEnabled(userType != BaseCelerClient::CelerUserType::Market);
   ui_->actionOneTimePassword->setEnabled(true);
   ui_->actionEnterColorCoinToken->setEnabled(true);

   ui_->actionDeposits->setEnabled(true);
   ui_->actionWithdrawalRequest->setEnabled(true);
   ui_->actionLinkAdditionalBankAccount->setEnabled(true);

   actLogin_->setVisible(false);
   actLogout_->setVisible(true);

   //   ccFileManager_->ConnectToCelerClient(celerConnection_);
   ui_->widgetRFQ->onMatchingLogin(mtchLogin, userType, userId);
   ui_->widgetRFQReply->onUserConnected(userType);

   statusBarView_->onConnectedToMatching();
   orderListModel_->onMatchingLogin();
}

void bs::gui::qt::MainWindow::onLogoutInitiated()
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
}

void MainWindow::onLoggedOut()
{
   currentUserLogin_.clear();
   emit needMatchingLogout();
}

void MainWindow::onMatchingLogout()
{
   emit needCloseBsConnection();

   ui_->actionAccountInformation->setEnabled(false);
   ui_->actionAuthenticationAddresses->setEnabled(false);
   ui_->actionEnterColorCoinToken->setEnabled(false);
   ui_->actionOneTimePassword->setEnabled(false);

   ui_->actionDeposits->setEnabled(false);
   ui_->actionWithdrawalRequest->setEnabled(false);
   ui_->actionLinkAdditionalBankAccount->setEnabled(false);

   actLogin_->setVisible(true);
   actLogin_->setEnabled(true);
   actLogout_->setVisible(false);

   statusBarView_->onDisconnectedFromMatching();
   setLoginButtonText(loginButtonText_);

   if (orderListModel_) {
      orderListModel_->onMatchingLogout();
   }
   ui_->widgetRFQ->onMatchingLogout();
   ui_->widgetRFQReply->onMatchingLogout();
}

void bs::gui::qt::MainWindow::onNewSecurity(const std::string& name, bs::network::Asset::Type at)
{
   ui_->widgetRFQ->onNewSecurity(name, at);
}

void MainWindow::onMDUpdated(bs::network::Asset::Type assetType
   , const QString& security, const bs::network::MDFields &fields)
{
   ui_->widgetRFQ->onMDUpdated(assetType, security, fields);
   ui_->widgetRFQReply->onMDUpdated(assetType, security, fields);
   ui_->widgetPortfolio->onMDUpdated(assetType, security, fields);
}

void bs::gui::qt::MainWindow::onBalance(const std::string& currency, double balance)
{
   statusBarView_->onBalanceUpdated(currency, balance);
   ui_->widgetRFQ->onBalance(currency, balance);
   ui_->widgetRFQReply->onBalance(currency, balance);
}

void MainWindow::onAuthAddresses(const std::vector<bs::Address> &addrs
   , const std::map<bs::Address, AddressVerificationState>& states)
{
   if (authAddrDlg_) {
      authAddrDlg_->onAuthAddresses(addrs, states);
   }
}

void MainWindow::onSubmittedAuthAddresses(const std::vector<bs::Address>& addrs)
{
   if (authAddrDlg_) {
      authAddrDlg_->onSubmittedAuthAddresses(addrs);
   }
}

void MainWindow::onVerifiedAuthAddresses(const std::vector<bs::Address>& addrs)
{
   ui_->widgetRFQ->onVerifiedAuthAddresses(addrs);
   ui_->widgetRFQReply->onVerifiedAuthAddresses(addrs);
}

void MainWindow::onAuthKey(const bs::Address& addr, const BinaryData& authKey)
{
   ui_->widgetRFQ->onAuthKey(addr, authKey);
   ui_->widgetRFQReply->onAuthKey(addr, authKey);
}

void MainWindow::onQuoteReceived(const bs::network::Quote& quote)
{
   ui_->widgetRFQ->onQuoteReceived(quote);
}

void MainWindow::onQuoteMatched(const std::string& rfqId, const std::string &quoteId)
{
   ui_->widgetRFQ->onQuoteMatched(rfqId, quoteId);
   ui_->widgetRFQReply->onQuoteMatched(rfqId, quoteId);
}

void MainWindow::onQuoteFailed(const std::string& rfqId, const std::string& quoteId
   , const std::string &info)
{
   ui_->widgetRFQ->onQuoteFailed(rfqId, quoteId, info);
   ui_->widgetRFQReply->onQuoteFailed(rfqId, quoteId, info);
}

void bs::gui::qt::MainWindow::onSettlementPending(const std::string& rfqId
   , const std::string& quoteId, const BinaryData& settlementId, int timeLeftMS)
{
   ui_->widgetRFQ->onSettlementPending(rfqId, quoteId, settlementId, timeLeftMS);
   ui_->widgetRFQReply->onSettlementPending(rfqId, quoteId, settlementId, timeLeftMS);
}

void bs::gui::qt::MainWindow::onSettlementComplete(const std::string& rfqId
   , const std::string& quoteId, const BinaryData& settlementId)
{
   ui_->widgetRFQ->onSettlementComplete(rfqId, quoteId, settlementId);
   ui_->widgetRFQReply->onSettlementComplete(rfqId, quoteId, settlementId);
}

void bs::gui::qt::MainWindow::onQuoteReqNotification(const bs::network::QuoteReqNotification& qrn)
{
   ui_->widgetRFQReply->onQuoteReqNotification(qrn);
}

void MainWindow::onOrdersUpdate(const std::vector<bs::network::Order>& orders)
{
   orderListModel_->onOrdersUpdate(orders);
}

void bs::gui::qt::MainWindow::onQuoteCancelled(const std::string& rfqId
   , const std::string& quoteId, bool byUser)
{
   ui_->widgetRFQReply->onQuoteCancelled(QString::fromStdString(rfqId), byUser);
}

void MainWindow::onReservedUTXOs(const std::string& resId
   , const std::string& subId, const std::vector<UTXO>& utxos)
{
   ui_->widgetRFQ->onReservedUTXOs(resId, subId, utxos);
   ui_->widgetRFQReply->onReservedUTXOs(resId, subId, utxos);
}

void MainWindow::showRunInBackgroundMessage()
{
   sysTrayIcon_->showMessage(tr("BlockSettle is running")
      , tr("BlockSettle Terminal is running in the backgroud. Click the tray icon to open the main window.")
      , QIcon(QLatin1String(":/resources/login-logo.png")));
}

void MainWindow::closeEvent(QCloseEvent* event)
{
   emit putSetting(ApplicationSettings::GUI_main_geometry, geometry());
   emit putSetting(ApplicationSettings::GUI_main_tab, ui_->tabWidget->currentIndex());

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
      QTimer::singleShot(100, [] { QApplication::exit(); });
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
      onLoginInitiated();
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
   ui_->widgetWallets->init(logger_);
   connect(ui_->widgetWallets, &WalletsWidget::newWalletCreationRequest, this, &MainWindow::createNewWallet);
   connect(ui_->widgetWallets, &WalletsWidget::needHDWalletDetails, this, &MainWindow::needHDWalletDetails);
   connect(ui_->widgetWallets, &WalletsWidget::needWalletBalances, this, &MainWindow::needWalletBalances);
   connect(ui_->widgetWallets, &WalletsWidget::needUTXOs, this, &MainWindow::needUTXOs);
   connect(ui_->widgetWallets, &WalletsWidget::needExtAddresses, this, &MainWindow::needExtAddresses);
   connect(ui_->widgetWallets, &WalletsWidget::needIntAddresses, this, &MainWindow::needIntAddresses);
   connect(ui_->widgetWallets, &WalletsWidget::needUsedAddresses, this, &MainWindow::needUsedAddresses);
   connect(ui_->widgetWallets, &WalletsWidget::needAddrComments, this, &MainWindow::needAddrComments);
   connect(ui_->widgetWallets, &WalletsWidget::setAddrComment, this, &MainWindow::setAddrComment);
   connect(ui_->widgetWallets, &WalletsWidget::needLedgerEntries, this, &MainWindow::needLedgerEntries);
   connect(ui_->widgetWallets, &WalletsWidget::needTXDetails, this, &MainWindow::needTXDetails);
   connect(ui_->widgetWallets, &WalletsWidget::needWalletDialog, this, &MainWindow::needWalletDialog);

   connect(ui_->widgetExplorer, &ExplorerWidget::needAddressHistory, this, &MainWindow::needAddressHistory);
   connect(ui_->widgetExplorer, &ExplorerWidget::needTXDetails, this, &MainWindow::needTXDetails);

   initTransactionsView();

   /*   portfolioModel_ = std::make_shared<CCPortfolioModel>(walletsMgr_, assetManager_, this);
      ui_->widgetPortfolio->init(applicationSettings_, mdProvider_, mdCallbacks_
         , portfolioModel_, signContainer_, armory_, utxoReservationMgr_, logMgr_->logger("ui"), walletsMgr_);*/

   orderListModel_ = std::make_shared<OrderListModel>(this);
   dialogMgr_ = std::make_shared<DialogManager>(this);

   ui_->widgetRFQ->init(logger_, dialogMgr_, orderListModel_.get());
   connect(ui_->widgetRFQ, &RFQRequestWidget::requestPrimaryWalletCreation, this, &MainWindow::createNewWallet);
   connect(ui_->widgetRFQ, &RFQRequestWidget::needWalletData, this, &MainWindow::needWalletData);
   connect(ui_->widgetRFQ, &RFQRequestWidget::loginRequested, this, &MainWindow::onLoginInitiated);
   connect(ui_->widgetRFQ, &RFQRequestWidget::needSubmitRFQ, this, &MainWindow::needSubmitRFQ);
   connect(ui_->widgetRFQ, &RFQRequestWidget::needAcceptRFQ, this, &MainWindow::needAcceptRFQ);
   connect(ui_->widgetRFQ, &RFQRequestWidget::needCancelRFQ, this, &MainWindow::needCancelRFQ);
   connect(ui_->widgetRFQ, &RFQRequestWidget::needAuthKey, this, &MainWindow::needAuthKey);
   connect(ui_->widgetRFQ, &RFQRequestWidget::needReserveUTXOs, this, &MainWindow::needReserveUTXOs);

   connect(ui_->widgetRFQReply, &RFQReplyWidget::requestPrimaryWalletCreation, this
      , &MainWindow::createNewWallet);
   connect(ui_->widgetRFQReply, &RFQReplyWidget::putSetting, this, &MainWindow::putSetting);
   connect(ui_->widgetRFQReply, &RFQReplyWidget::submitQuote, this, &MainWindow::submitQuote);
   connect(ui_->widgetRFQReply, &RFQReplyWidget::pullQuote, this, &MainWindow::pullQuote);
   connect(ui_->widgetRFQReply, &RFQReplyWidget::needAuthKey, this, &MainWindow::needAuthKey);
   connect(ui_->widgetRFQReply, &RFQReplyWidget::needReserveUTXOs, this, &MainWindow::needReserveUTXOs);
   ui_->widgetRFQReply->init(logger_, orderListModel_.get());

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
