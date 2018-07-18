#ifndef __STATUS_BAR_VIEW_H__
#define __STATUS_BAR_VIEW_H__

#include <QObject>
#include <QStatusBar>
#include <QLabel>
#include <QProgressBar>
#include <QPixmap>
#include <QIcon>

#include <memory>
#include "ArmoryConnection.h"
#include "CelerClient.h"

class AssetManager;
class SignContainer;
class WalletsManager;

class StatusBarView  : public QObject
{
   Q_OBJECT
public:
   StatusBarView(const std::shared_ptr<ArmoryConnection> &, std::shared_ptr<WalletsManager> walletsManager
      , std::shared_ptr<AssetManager> assetManager, const std::shared_ptr<CelerClient> &
      , const std::shared_ptr<SignContainer> &, QStatusBar *parent);
   ~StatusBarView() noexcept override = default;

   StatusBarView(const StatusBarView&) = delete;
   StatusBarView& operator = (const StatusBarView&) = delete;
   StatusBarView(StatusBarView&&) = delete;
   StatusBarView& operator = (StatusBarView&&) = delete;

private slots:
   void onPrepareArmoryConnection(NetworkType, std::string host, std::string port);
   void onArmoryStateChanged(ArmoryConnection::State);
   void onArmoryProgress(BDMPhase, float progress, unsigned int secondsRem, unsigned int numProgress);
   void onArmoryError(QString);
   void onConnectedToServer();
   void onConnectionClosed();
   void onConnectionError(int errorCode);
   void onContainerConnected();
   void onContainerDisconnected();
   void onContainerAuthorized();
   void onContainerError();
   void updateBalances();
   void onWalletImportStarted(const std::string &walletId);
   void onWalletImportFinished(const std::string &walletId);

public:
   void updateDBHeadersProgress(float progress, unsigned secondsRem);
   void updateOrganizingChainProgress(float progress, unsigned secondsRem);
   void updateBlockHeadersProgress(float progress, unsigned secondsRem);
   void updateBlockDataProgress(float progress, unsigned secondsRem);
   void updateRescanProgress(float progress, unsigned secondsRem);

private:
   void setupBtcIcon(NetworkType);
   void SetLoggedinStatus();
   void SetCelerErrorStatus(const QString& message);
   void SetLoggedOutStatus();
   void SetCelerConnectingStatus();
   QWidget *CreateSeparator();

private:
   void updateProgress(float progress, unsigned secondsRem);
   QString getImportingText() const;

private:
   QStatusBar     *statusBar_;

   QLabel         *estimateLabel_;
   QLabel         *balanceLabel_;
   QLabel         *celerConnectionIconLabel_;
   QLabel         *connectionStatusLabel_;
   QLabel         *containerStatusLabel_;
   QProgressBar   *progressBar_;

   const QSize iconSize_;
   QIcon       iconCeler_;
   QPixmap     iconOffline_;
   QPixmap     iconError_;
   QPixmap     iconOnline_;
   QPixmap     iconConnecting_;
   QPixmap     iconCelerOffline_;
   QPixmap     iconCelerError_;
   QPixmap     iconCelerOnline_;
   QPixmap     iconCelerConnecting_;
   QPixmap     iconContainerOffline_;
   QPixmap     iconContainerError_;
   QPixmap     iconContainerConnecting_;
   QPixmap     iconContainerOnline_;

   std::shared_ptr<ArmoryConnection>   armory_;
   std::shared_ptr<WalletsManager>     walletsManager_;
   std::shared_ptr<AssetManager>       assetManager_;
   std::unordered_set<std::string>     importingWallets_;
};

#endif // __STATUS_BAR_VIEW_H__
