/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __STATUS_BAR_VIEW_H__
#define __STATUS_BAR_VIEW_H__

#include <QObject>
#include <QStatusBar>
#include <QLabel>
#include <QPixmap>
#include <QIcon>

#include <memory>
#include "ArmoryConnection.h"
#include "CelerClient.h"
#include "CircleProgressBar.h"
#include "SignContainer.h"

namespace bs {
   namespace sync {
      class WalletsManager;
   }
}
class AssetManager;
class SignContainer;

class StatusBarView  : public QObject, public ArmoryCallbackTarget
{
   Q_OBJECT
public:
   StatusBarView(const std::shared_ptr<ArmoryConnection> &
      , const std::shared_ptr<bs::sync::WalletsManager> &
      , std::shared_ptr<AssetManager> assetManager, const std::shared_ptr<BaseCelerClient> &
      , const std::shared_ptr<SignContainer> &, QStatusBar *parent);
   ~StatusBarView() noexcept override;

   StatusBarView(const StatusBarView&) = delete;
   StatusBarView& operator = (const StatusBarView&) = delete;
   StatusBarView(StatusBarView&&) = delete;
   StatusBarView& operator = (StatusBarView&&) = delete;

private slots:
   void onPrepareArmoryConnection(NetworkType);
   void onArmoryStateChanged(ArmoryState);
   void onArmoryProgress(BDMPhase, float progress, unsigned int secondsRem);
   void onArmoryError(QString);
   void onConnectedToServer();
   void onConnectionClosed();
   void onConnectionError(int errorCode);
   void onContainerAuthorized();
   void onContainerError(SignContainer::ConnectionError error, const QString &details);
   void updateBalances();
   void onWalletImportStarted(const std::string &walletId);
   void onWalletImportFinished(const std::string &walletId);

private:
   void onStateChanged(ArmoryState) override;
   void onError(const std::string &, const std::string &) override;
   void onLoadProgress(BDMPhase, float, unsigned int, unsigned int) override;
   void onPrepareConnection(NetworkType, const std::string &host
      , const std::string &port) override;

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
   ArmoryState armoryConnState_;
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
   QPixmap     iconContainerOnline_;

   std::shared_ptr<bs::sync::WalletsManager> walletsManager_;
   std::shared_ptr<AssetManager>       assetManager_;
   std::unordered_set<std::string>     importingWallets_;
};

#endif // __STATUS_BAR_VIEW_H__
