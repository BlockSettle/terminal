#include "StatusBarView.h"
#include "AssetManager.h"
#include "HDWallet.h"
#include "PyBlockDataManager.h"
#include "SignContainer.h"
#include "UiUtils.h"
#include "WalletsManager.h"


class StatusViewBlockListener : public PyBlockDataListener
{
public:
   StatusViewBlockListener(StatusBarView* parent)
      : statusBarView_(parent)
   {}

   ~StatusViewBlockListener() noexcept override = default;

   void StateChanged(PyBlockDataManagerState newState) override {
      switch(newState)
      {
      case PyBlockDataManagerState::Offline:
         statusBarView_->onSetOfflineStatus();
         break;
      case PyBlockDataManagerState::Scaning:
         statusBarView_->onSetConnectingStatus();
         break;
      case PyBlockDataManagerState::Error:
         statusBarView_->onSetErrorStatus(QLatin1String("Error"));
         break;
      case PyBlockDataManagerState::Closing:
         statusBarView_->onSetOfflineStatus();
         break;
      case PyBlockDataManagerState::Ready:
         statusBarView_->onSetInitializingCompleteStatus();
         break;
      default: break;
      }
   }

   void ProgressUpdated(BDMPhase phase, const vector<string> &walletIdVec, float progress,unsigned secondsRem, unsigned progressNumeric) override
   {
      Q_UNUSED(walletIdVec);
      Q_UNUSED(progressNumeric);
      switch(phase) {
      case BDMPhase_DBHeaders:
         statusBarView_->onUpdateDBHeadersProgress(progress, secondsRem);
         break;
      case BDMPhase_OrganizingChain:
         statusBarView_->onUpdateOrganizingChainProgress(progress, secondsRem);
         break;
      case BDMPhase_BlockHeaders:
         statusBarView_->onUpdateBlockHeadersProgress(progress, secondsRem);
         break;
      case BDMPhase_BlockData:
         statusBarView_->onUpdateBlockDataProgress(progress, secondsRem);
         break;
      case BDMPhase_Rescan:
         statusBarView_->onUpdateRescanProgress(progress, secondsRem);
         break;
      default: break;
      }
   }
private:
   StatusBarView *statusBarView_;
};

StatusBarView::StatusBarView(const std::shared_ptr<PyBlockDataManager>& bdm, std::shared_ptr<WalletsManager> walletsManager, std::shared_ptr<AssetManager> assetManager, QStatusBar *parent)
   : QObject(parent)
   , statusBar_(parent)
   , bdm_(bdm)
   , walletsManager_(walletsManager)
   , listener_(nullptr)
   , assetManager_(assetManager)
{

   QString windowIconName;
   if (bdm_->networkType() == NetworkType::TestNet)
   {
      windowIconName = QLatin1String("_TESTNET");
   }
   for (int s : {16, 24, 32})
   {
      iconCeler_.addFile(QString(QLatin1String(":/ICON_BS_%1")).arg(s), QSize(s, s), QIcon::Normal);
      iconCeler_.addFile(QString(QLatin1String(":/ICON_BS_%1_GRAY")).arg(s), QSize(s, s), QIcon::Disabled);
   }
   estimateLabel_ = new QLabel(statusBar_);

   QSize iconSize(16, 16);
   iconCelerOffline_ = iconCeler_.pixmap(iconSize, QIcon::Disabled);
   iconCelerConnecting_ = iconCeler_.pixmap(iconSize, QIcon::Disabled);
   iconCelerOnline_ = iconCeler_.pixmap(iconSize, QIcon::Normal);
   iconCelerError_ = iconCeler_.pixmap(iconSize, QIcon::Disabled);

   QIcon btcIcon(QLatin1String(":/ICON_BITCOIN") + windowIconName);
   QIcon btcIconGray(QLatin1String(":/ICON_BITCOIN_GRAY"));

   iconOffline_ = btcIconGray.pixmap(iconSize);
   iconError_ = btcIconGray.pixmap(iconSize);
   iconOnline_ = btcIcon.pixmap(iconSize);
   iconConnecting_ = btcIconGray.pixmap(iconSize);

   QIcon contIconGray(QLatin1String(":/ICON_STATUS_OFFLINE"));
   QIcon contIconYellow(QLatin1String(":/ICON_STATUS_CONNECTING"));
   QIcon contIconRed(QLatin1String(":/ICON_STATUS_ERROR"));
   QIcon contIconGreen(QLatin1String(":/ICON_STATUS_ONLINE"));

   iconContainerOffline_ = contIconGray.pixmap(iconSize);
   iconContainerError_ = contIconRed.pixmap(iconSize);
   iconContainerOnline_ = contIconGreen.pixmap(iconSize);
   iconContainerConnecting_ = contIconYellow.pixmap(iconSize);

   balanceLabel_ = new QLabel(statusBar_);
   balanceLabel_->setVisible(false);

   progressBar_ = new CircleProgressBar(statusBar_);
   progressBar_->setMinimum(0);
   progressBar_->setMaximum(100);
   progressBar_->hide();

   celerConnectionIconLabel_ = new QLabel(statusBar_);
   connectionStatusLabel_ = new QLabel(statusBar_);

   containerStatusLabel_ = new QLabel(statusBar_);
   containerStatusLabel_->setPixmap(iconContainerOffline_);
   containerStatusLabel_->setToolTip(tr("Signing container status"));

   statusBar_->addWidget(estimateLabel_);
   statusBar_->addWidget(balanceLabel_);

   statusBar_->addPermanentWidget(celerConnectionIconLabel_);
   statusBar_->addPermanentWidget(CreateSeparator());
   statusBar_->addPermanentWidget(progressBar_);
   statusBar_->addPermanentWidget(connectionStatusLabel_);
   statusBar_->addPermanentWidget(CreateSeparator());
   statusBar_->addPermanentWidget(containerStatusLabel_);

   QWidget* w = new QWidget();
   w->setFixedWidth(6);
   statusBar_->addPermanentWidget(w);

   statusBar_->setStyleSheet(QLatin1String("QStatusBar::item { border: none; }"));

   setOfflineStatus();
   SetLoggedOutStatus();

   connect(this, &StatusBarView::onSetOfflineStatus, this, &StatusBarView::setOfflineStatus);
   connect(this, &StatusBarView::onSetConnectingStatus, this, &StatusBarView::setConnectingStatus);
   connect(this, &StatusBarView::onSetErrorStatus, this, &StatusBarView::setErrorStatus);
   connect(this, &StatusBarView::onSetInitializingCompleteStatus, this, &StatusBarView::setInitializingCompleteStatus);

   connect(this, &StatusBarView::onUpdateDBHeadersProgress, this, &StatusBarView::updateDBHeadersProgress);
   connect(this, &StatusBarView::onUpdateOrganizingChainProgress, this, &StatusBarView::updateOrganizingChainProgress);
   connect(this, &StatusBarView::onUpdateBlockHeadersProgress, this, &StatusBarView::updateBlockHeadersProgress);
   connect(this, &StatusBarView::onUpdateBlockDataProgress, this, &StatusBarView::updateBlockDataProgress);
   connect(this, &StatusBarView::onUpdateRescanProgress, this, &StatusBarView::updateRescanProgress);

   connect(assetManager_.get(), &AssetManager::totalChanged, this, &StatusBarView::updateBalances);
   connect(assetManager_.get(), &AssetManager::securitiesChanged, this, &StatusBarView::updateBalances);

   connect(walletsManager_.get(), &WalletsManager::walletImportStarted, this, &StatusBarView::onWalletImportStarted);
   connect(walletsManager_.get(), &WalletsManager::walletImportFinished, this, &StatusBarView::onWalletImportFinished);

   if (bdm_) {
      listener_ = std::make_shared<StatusViewBlockListener>(this);
      bdm_->addListener(listener_.get());
   }
}

QWidget *StatusBarView::CreateSeparator()
{
   const auto separator = new QWidget(statusBar_);
   separator->setFixedWidth(1);
   separator->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
   separator->setStyleSheet(QLatin1String("background-color: #939393;"));
   return separator;
}

void StatusBarView::setConnectingStatus()
{
   progressBar_->setVisible(false);
   estimateLabel_->setVisible(false);
   connectionStatusLabel_->show();

   connectionStatusLabel_->setToolTip(tr("Connecting..."));
   connectionStatusLabel_->setPixmap(iconConnecting_);
}

void StatusBarView::setInitializingCompleteStatus()
{
   progressBar_->setVisible(false);
   estimateLabel_->setVisible(false);
   connectionStatusLabel_->show();

   connectionStatusLabel_->setToolTip(tr("Connected to DB (%1 blocks)").arg(walletsManager_->GetTopBlockHeight()));
   connectionStatusLabel_->setPixmap(iconOnline_);

   balanceLabel_->setVisible(true);
   updateBalances();
}

void StatusBarView::updateBalances()
{
   QString text = tr("   XBT: <b>%1</b> ").arg(UiUtils::displayAmount(walletsManager_->GetSpendableBalance()));
   for (const auto& currency : assetManager_->currencies()) {
      text += tr("| %1: <b>%2</b> ")
         .arg(QString::fromStdString(currency))
         .arg(UiUtils::displayCurrencyAmount(assetManager_->getBalance(currency)));
   }

   balanceLabel_->setText(text);
}

void StatusBarView::setErrorStatus(const QString& errorMessage)
{
   progressBar_->setVisible(false);
   estimateLabel_->setVisible(false);
   connectionStatusLabel_->show();

   connectionStatusLabel_->setToolTip(errorMessage);
   connectionStatusLabel_->setPixmap(iconError_);
}

void StatusBarView::setOfflineStatus()
{
   progressBar_->setVisible(false);
   estimateLabel_->setVisible(false);
   connectionStatusLabel_->show();

   connectionStatusLabel_->setToolTip(tr("Database Offline"));
   connectionStatusLabel_->setPixmap(iconOffline_);
}

void StatusBarView::updateDBHeadersProgress(float progress, unsigned secondsRem)
{
   progressBar_->setToolTip(tr("Loading DB headers"));
   updateProgress(progress, secondsRem);
}

void StatusBarView::updateOrganizingChainProgress(float progress, unsigned secondsRem)
{
   progressBar_->setToolTip(tr("Organizing blockchains"));
   updateProgress(progress, secondsRem);
}

void StatusBarView::updateBlockHeadersProgress(float progress, unsigned secondsRem)
{
   progressBar_->setToolTip(tr("Reading new block headers"));
   updateProgress(progress, secondsRem);
}

void StatusBarView::updateBlockDataProgress(float progress, unsigned secondsRem)
{
   progressBar_->setToolTip(tr("Building databases"));
   updateProgress(progress, secondsRem);
}

void StatusBarView::updateRescanProgress(float progress, unsigned secondsRem)
{
   progressBar_->setToolTip(tr("Rescanning databases"));
   updateProgress(progress, secondsRem);
}

void StatusBarView::updateProgress(float progress, unsigned)
{
   progressBar_->setVisible(true);
   estimateLabel_->setVisible(true);
   connectionStatusLabel_->hide();

   progressBar_->setValue(progress * 100);
}

void StatusBarView::SetLoggedinStatus()
{
   celerConnectionIconLabel_->setToolTip(tr("Logged in to Matching System"));
   celerConnectionIconLabel_->setPixmap(iconCelerOnline_);
}

void StatusBarView::SetLoggedOutStatus()
{
   celerConnectionIconLabel_->setToolTip(tr("Logged out of Matching System"));
   celerConnectionIconLabel_->setPixmap(iconCelerOffline_);
}

void StatusBarView::SetCelerConnectingStatus()
{
   celerConnectionIconLabel_->setToolTip(tr("Connecting to Matching System"));
   celerConnectionIconLabel_->setPixmap(iconCelerConnecting_);
}

void StatusBarView::connectToCelerClient(const std::shared_ptr<CelerClient>& celerClient)
{
   connect(celerClient.get(), &CelerClient::OnConnectedToServer, this, &StatusBarView::onConnectedToServer);
   connect(celerClient.get(), &CelerClient::OnConnectionClosed, this, &StatusBarView::onConnectionClosed);
   connect(celerClient.get(), &CelerClient::OnConnectionError, this, &StatusBarView::onConnectionError);
}

void StatusBarView::connectToContainer(const std::shared_ptr<SignContainer> &container)
{
   connect(container.get(), &SignContainer::connected, this, &StatusBarView::onContainerConnected);
   connect(container.get(), &SignContainer::disconnected, this, &StatusBarView::onContainerDisconnected);
   connect(container.get(), &SignContainer::authenticated, this, &StatusBarView::onContainerAuthorized);
   connect(container.get(), &SignContainer::connectionError, this, &StatusBarView::onContainerError);
}

void StatusBarView::onConnectedToServer()
{
   SetLoggedinStatus();
}

void StatusBarView::onConnectionClosed()
{
   SetLoggedOutStatus();
}

void StatusBarView::onConnectionError(int errorCode)
{
   switch(errorCode)
   {
   case CelerClient::ResolveHostError:
      statusBar_->showMessage(tr("Could not resolve Celer host"));
      break;
   case CelerClient::LoginError:
      statusBar_->showMessage(tr("Invalid login/password pair"), 2000);
      break;
   case CelerClient::ServerMaintainanceError:
      statusBar_->showMessage(tr("Server maintainance"));
      break;
   case CelerClient::UndefinedError:
      break;
   }
}

void StatusBarView::onContainerConnected()
{
   containerStatusLabel_->setPixmap(iconContainerConnecting_);
}

void StatusBarView::onContainerDisconnected()
{
   containerStatusLabel_->setPixmap(iconContainerOffline_);
}

void StatusBarView::onContainerAuthorized()
{
   containerStatusLabel_->setPixmap(iconContainerOnline_);
}

void StatusBarView::onContainerError()
{
   containerStatusLabel_->setPixmap(iconContainerError_);
}

void StatusBarView::onWalletImportStarted(const std::string &walletId)
{
   importingWallets_.insert(walletId);
   estimateLabel_->setVisible(true);
   estimateLabel_->setText(getImportingText());
}

void StatusBarView::onWalletImportFinished(const std::string &walletId)
{
   importingWallets_.erase(walletId);
   if (importingWallets_.empty()) {
      estimateLabel_->clear();
      estimateLabel_->setVisible(false);
   }
   else {
      estimateLabel_->setText(getImportingText());
   }
}

QString StatusBarView::getImportingText() const
{
   if (importingWallets_.empty()) {
      return {};
   }
   if (importingWallets_.size() == 1) {
      return tr("Rescanning blockchain for wallet %1...").arg(QString::fromStdString(walletsManager_->GetHDWalletById(*(importingWallets_.begin()))->getName()));
   }
   else {
      QStringList walletNames;
      for (const auto &walletId : importingWallets_) {
         walletNames << QString::fromStdString(walletsManager_->GetHDWalletById(*(importingWallets_.begin()))->getName());
      }
      return tr("Rescanning blockchain for wallets %1...").arg(walletNames.join(QLatin1Char(',')));
   }
}
