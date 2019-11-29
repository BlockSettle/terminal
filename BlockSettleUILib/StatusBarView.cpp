/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "StatusBarView.h"
#include "AssetManager.h"
#include "UiUtils.h"
#include "Wallets/SyncHDWallet.h"
#include "Wallets/SyncWalletsManager.h"


StatusBarView::StatusBarView(const std::shared_ptr<ArmoryConnection> &armory
   , const std::shared_ptr<bs::sync::WalletsManager> &walletsManager
   , std::shared_ptr<AssetManager> assetManager, const std::shared_ptr<BaseCelerClient> &celerClient
   , const std::shared_ptr<SignContainer> &container, QStatusBar *parent)
   : QObject(nullptr)
   , statusBar_(parent)
   , iconSize_(16, 16)
   , armoryConnState_(ArmoryState::Offline)
   , walletsManager_(walletsManager)
   , assetManager_(assetManager)
{
   init(armory.get());

   for (int s : {16, 24, 32})
   {
      iconCeler_.addFile(QString(QLatin1String(":/ICON_BS_%1")).arg(s), QSize(s, s), QIcon::Normal);
      iconCeler_.addFile(QString(QLatin1String(":/ICON_BS_%1_GRAY")).arg(s), QSize(s, s), QIcon::Disabled);
   }
   estimateLabel_ = new QLabel(statusBar_);

   iconCelerOffline_ = iconCeler_.pixmap(iconSize_, QIcon::Disabled);
   iconCelerConnecting_ = iconCeler_.pixmap(iconSize_, QIcon::Disabled);
   iconCelerOnline_ = iconCeler_.pixmap(iconSize_, QIcon::Normal);
   iconCelerError_ = iconCeler_.pixmap(iconSize_, QIcon::Disabled);

   QIcon contIconGray(QLatin1String(":/ICON_STATUS_OFFLINE"));
   QIcon contIconYellow(QLatin1String(":/ICON_STATUS_CONNECTING"));
   QIcon contIconRed(QLatin1String(":/ICON_STATUS_ERROR"));
   QIcon contIconGreen(QLatin1String(":/ICON_STATUS_ONLINE"));

   iconContainerOffline_ = contIconGray.pixmap(iconSize_);
   iconContainerError_ = contIconRed.pixmap(iconSize_);
   iconContainerOnline_ = contIconGreen.pixmap(iconSize_);

   balanceLabel_ = new QLabel(statusBar_);

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

   SetLoggedOutStatus();

   connect(assetManager_.get(), &AssetManager::totalChanged, this, &StatusBarView::updateBalances);
   connect(assetManager_.get(), &AssetManager::securitiesChanged, this, &StatusBarView::updateBalances);

   connect(walletsManager_.get(), &bs::sync::WalletsManager::walletImportStarted, this, &StatusBarView::onWalletImportStarted);
   connect(walletsManager_.get(), &bs::sync::WalletsManager::walletImportFinished, this, &StatusBarView::onWalletImportFinished);
   connect(walletsManager_.get(), &bs::sync::WalletsManager::walletBalanceUpdated, this, &StatusBarView::updateBalances);

   connect(celerClient.get(), &BaseCelerClient::OnConnectedToServer, this, &StatusBarView::onConnectedToServer);
   connect(celerClient.get(), &BaseCelerClient::OnConnectionClosed, this, &StatusBarView::onConnectionClosed);
   connect(celerClient.get(), &BaseCelerClient::OnConnectionError, this, &StatusBarView::onConnectionError);

   // container might be null if user rejects remote signer key
   if (container) {
      // connected are not used here because we wait for authenticated signal instead
      // disconnected are not used here because onContainerError should be always called
      connect(container.get(), &SignContainer::authenticated, this, &StatusBarView::onContainerAuthorized);
      connect(container.get(), &SignContainer::connectionError, this, &StatusBarView::onContainerError);
   }

   onArmoryStateChanged(armory_->state());
   onConnectionClosed();
   setBalances();

   containerStatusLabel_->setPixmap(iconContainerOffline_);
}

StatusBarView::~StatusBarView() noexcept
{
   estimateLabel_->deleteLater();
   balanceLabel_->deleteLater();
   celerConnectionIconLabel_->deleteLater();
   connectionStatusLabel_->deleteLater();
   containerStatusLabel_->deleteLater();
   progressBar_->deleteLater();

   for (QWidget *separator : separators_) {
      separator->deleteLater();
   }

   cleanup();
}

void StatusBarView::onStateChanged(ArmoryState state)
{
   QMetaObject::invokeMethod(this, [this, state] { onArmoryStateChanged(state); });
}

void StatusBarView::onError(const std::string &msg, const std::string &)
{
   QMetaObject::invokeMethod(this, [this, msg] {
      onArmoryError(QString::fromStdString(msg));
   });
}

void StatusBarView::onLoadProgress(BDMPhase phase, float progress, unsigned int secs, unsigned int)
{
   QMetaObject::invokeMethod(this, [this, phase, progress, secs] {
      onArmoryProgress(phase, progress, secs);
   });
}

void StatusBarView::onPrepareConnection(NetworkType netType, const std::string &, const std::string &)
{
   QMetaObject::invokeMethod(this, [this, netType] { onPrepareArmoryConnection(netType); });
}

void StatusBarView::setupBtcIcon(NetworkType netType)
{
   QString iconSuffix;
   if (netType == NetworkType::TestNet) {
      iconSuffix = QLatin1String("_TESTNET");
   }

   QIcon btcIcon(QLatin1String(":/ICON_BITCOIN") + iconSuffix);
   QIcon btcIconGray(QLatin1String(":/ICON_BITCOIN_GRAY"));
   QIcon btcIconEnabled(QLatin1String(":/ICON_BITCOIN_ENABLED"));
   QIcon btcIconError(QLatin1String(":/ICON_BITCOIN_ERROR"));

   iconOffline_ = btcIconGray.pixmap(iconSize_);
   iconError_ = btcIconError.pixmap(iconSize_);
   iconOnline_ = btcIcon.pixmap(iconSize_);
   iconConnecting_ = btcIconEnabled.pixmap(iconSize_);
}

QWidget *StatusBarView::CreateSeparator()
{
   const auto separator = new QWidget(statusBar_);
   separator->setFixedWidth(1);
   separator->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
   separator->setStyleSheet(QLatin1String("background-color: #939393;"));
   separators_.append(separator);
   return separator;
}

void StatusBarView::onPrepareArmoryConnection(NetworkType netType)
{
   setupBtcIcon(netType);

   progressBar_->setVisible(false);
   estimateLabel_->setVisible(false);

   onArmoryStateChanged(ArmoryState::Offline);
}

void StatusBarView::onArmoryStateChanged(ArmoryState state)
{
   progressBar_->setVisible(false);
   estimateLabel_->setVisible(false);
   connectionStatusLabel_->show();

   armoryConnState_ = state;

   setBalances();

   switch (state) {
   case ArmoryState::Scanning:
   case ArmoryState::Connecting:
      connectionStatusLabel_->setToolTip(tr("Connecting..."));
      connectionStatusLabel_->setPixmap(iconConnecting_);
      break;

   case ArmoryState::Closing:
   case ArmoryState::Offline:
   case ArmoryState::Cancelled:
      connectionStatusLabel_->setToolTip(tr("Database Offline"));
      connectionStatusLabel_->setPixmap(iconOffline_);
      break;

   case ArmoryState::Ready:
      connectionStatusLabel_->setToolTip(tr("Connected to DB (%1 blocks)").arg(armory_->topBlock()));
      connectionStatusLabel_->setPixmap(iconOnline_);
      updateBalances();
      break;

   default:    break;
   }
}

void StatusBarView::onArmoryProgress(BDMPhase phase, float progress, unsigned int secondsRem)
{
   switch (phase) {
   case BDMPhase_DBHeaders:
      updateDBHeadersProgress(progress, secondsRem);
      break;
   case BDMPhase_OrganizingChain:
      updateOrganizingChainProgress(progress, secondsRem);
      break;
   case BDMPhase_BlockHeaders:
      updateBlockHeadersProgress(progress, secondsRem);
      break;
   case BDMPhase_BlockData:
      updateBlockDataProgress(progress, secondsRem);
      break;
   case BDMPhase_Rescan:
      updateRescanProgress(progress, secondsRem);
      break;
   default: break;
   }
}

void StatusBarView::onArmoryError(QString errorMessage)
{
   progressBar_->setVisible(false);
   estimateLabel_->setVisible(false);
   connectionStatusLabel_->show();

   connectionStatusLabel_->setToolTip(errorMessage);
   connectionStatusLabel_->setPixmap(iconError_);
}

void StatusBarView::setBalances()
{
   QString xbt;

   switch (armoryConnState_) {
      case ArmoryState::Ready :
         xbt = UiUtils::displayAmount(walletsManager_->getTotalBalance());
      break;

      case ArmoryState::Scanning :
      case ArmoryState::Connected :
         xbt = tr("Loading...");
      break;

      case ArmoryState::Closing :
      case ArmoryState::Offline :
         xbt = tr("...");
      break;

      default :
         xbt = tr("...");
   }

   QString text = tr("   XBT: <b>%1</b> ").arg(xbt);

   for (const auto& currency : assetManager_->currencies()) {
      text += tr("| %1: <b>%2</b> ")
         .arg(QString::fromStdString(currency))
         .arg(UiUtils::displayCurrencyAmount(assetManager_->getBalance(currency)));
   }

   balanceLabel_->setText(text);
}

void StatusBarView::updateBalances()
{
   setBalances();

   progressBar_->setVisible(false);
   estimateLabel_->setVisible(false);
   connectionStatusLabel_->show();
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
   case BaseCelerClient::ResolveHostError:
      statusBar_->showMessage(tr("Could not resolve Celer host"));
      break;
   case BaseCelerClient::LoginError:
      statusBar_->showMessage(tr("Invalid login/password pair"), 2000);
      break;
   case BaseCelerClient::ServerMaintainanceError:
      statusBar_->showMessage(tr("Server maintainance"));
      break;
   case BaseCelerClient::UndefinedError:
      break;
   }
}

void StatusBarView::onContainerAuthorized()
{
   containerStatusLabel_->setPixmap(iconContainerOnline_);
}

void StatusBarView::onContainerError(SignContainer::ConnectionError error, const QString &details)
{
   Q_UNUSED(details);

   switch (error) {
      case SignContainer::NoError:
         assert(false);
         break;

      case SignContainer::UnknownError:
      case SignContainer::SocketFailed:
      case SignContainer::HostNotFound:
      case SignContainer::HandshakeFailed:
      case SignContainer::SerializationFailed:
      case SignContainer::HeartbeatWaitFailed:
      case SignContainer::InvalidProtocol:
      case SignContainer::NetworkTypeMismatch:
         containerStatusLabel_->setPixmap(iconContainerError_);
         break;

      case SignContainer::ConnectionTimeout:
      case SignContainer::SignerGoesOffline:
         containerStatusLabel_->setPixmap(iconContainerOffline_);
         break;
   }
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
   // Sometimes GetHDWalletById returns nullptr (perhaps importingWallets_ is stalled).
   // So add some error checking here.
   if (importingWallets_.size() == 1) {
      auto wallet = walletsManager_->getHDWalletById(*(importingWallets_.begin()));
      if (!wallet) {
         return {};
      }
      return tr("Rescanning blockchain for wallet %1...").arg(QString::fromStdString(wallet->name()));
   }
   else {
      QStringList walletNames;
      for (const auto &walletId : importingWallets_) {
         auto wallet = walletsManager_->getHDWalletById(walletId);
         if (wallet) {
            walletNames << QString::fromStdString(wallet->name());
         }
      }
      if (walletNames.empty()) {
         return {};
      }
      return tr("Rescanning blockchain for wallets %1...").arg(walletNames.join(QLatin1Char(',')));
   }
}
