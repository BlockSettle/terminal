#ifndef __STATUS_BAR_VIEW_H__
#define __STATUS_BAR_VIEW_H__

#include <QObject>
#include <QStatusBar>
#include <QLabel>
#include <QPixmap>
#include <QIcon>

#include <memory>
#include "ArmoryObject.h"
#include "CelerClient.h"
#include "CircleProgressBar.h"

namespace bs {
   namespace sync {
      class WalletsManager;
   }
}
class AssetManager;
class SignContainer;

class StatusBarView  : public QObject
{
   Q_OBJECT
public:
   StatusBarView(const std::shared_ptr<ArmoryObject> &
      , const std::shared_ptr<bs::sync::WalletsManager> &
      , std::shared_ptr<AssetManager> assetManager, const std::shared_ptr<CelerClient> &
      , const std::shared_ptr<SignContainer> &, QStatusBar *parent);
   ~StatusBarView() noexcept override;

   StatusBarView(const StatusBarView&) = delete;
   StatusBarView& operator = (const StatusBarView&) = delete;
   StatusBarView(StatusBarView&&) = delete;
   StatusBarView& operator = (StatusBarView&&) = delete;

private slots:
   void onPrepareArmoryConnection(const ArmorySettings &server);
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
   void setBalances();

private:
   void updateProgress(float progress, unsigned secondsRem);
   QString getImportingText() const;

private:
   QStatusBar     *statusBar_;

   QLabel            *estimateLabel_;
   QLabel            *balanceLabel_;
   QLabel            *celerConnectionIconLabel_;
   QLabel            *connectionStatusLabel_;
   QLabel            *containerStatusLabel_;
   CircleProgressBar *progressBar_;
   QVector<QWidget *> separators_;

   const QSize iconSize_;
   ArmoryConnection::State armoryConnState_;
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

   std::shared_ptr<ArmoryObject>   armory_;
   std::shared_ptr<bs::sync::WalletsManager> walletsManager_;
   std::shared_ptr<AssetManager>       assetManager_;
   std::unordered_set<std::string>     importingWallets_;
};

#endif // __STATUS_BAR_VIEW_H__
