/*

***********************************************************************************
* Copyright (C) 2018 - 2020, BlockSettle AB
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
#include "Celer/CelerClient.h"
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
   [[deprecated]] StatusBarView(const std::shared_ptr<ArmoryConnection> &
      , const std::shared_ptr<bs::sync::WalletsManager> &
      , std::shared_ptr<AssetManager> assetManager, const std::shared_ptr<CelerClientQt> &
      , const std::shared_ptr<SignContainer> &, QStatusBar *parent);
   StatusBarView(QStatusBar *parent);
   ~StatusBarView() noexcept override;

   StatusBarView(const StatusBarView&) = delete;
   StatusBarView& operator = (const StatusBarView&) = delete;
   StatusBarView(StatusBarView&&) = delete;
   StatusBarView& operator = (StatusBarView&&) = delete;

public slots:
   void onBalanceUpdated(const std::string &symbol, double balance);
   void onPrepareArmoryConnection(NetworkType);
   void onArmoryStateChanged(ArmoryState, unsigned int topBlock);
   void onArmoryProgress(BDMPhase, float progress, unsigned int secondsRem);
   void onArmoryError(QString);
   void onConnectedToMatching();
   void onDisconnectedFromMatching();
   void onMatchingConnError(int errorCode);
   void onContainerAuthorized();
   void onSignerStatusChanged(SignContainer::ConnectionError error, const QString &details);
   void updateBalances();  //deprecated
   void onWalletImportStarted(const std::string &walletId);
   void onWalletImportFinished(const std::string &walletId);
   void onBlockchainStateChanged(int, unsigned int);
   void onXbtBalance(const bs::sync::WalletBalanceData&);

private:
   [[deprecated]] void onStateChanged(ArmoryState) override;
   [[deprecated]] void onError(int errCode, const std::string &) override;
   [[deprecated]] void onLoadProgress(BDMPhase, float, unsigned int, unsigned int) override;
   [[deprecated]] void onPrepareConnection(NetworkType, const std::string &host
      , const std::string &port) override;
   [[deprecated]] void onNewBlock(unsigned, unsigned) override;

public:
   void updateDBHeadersProgress(float progress, unsigned secondsRem);
   void updateOrganizingChainProgress(float progress, unsigned secondsRem);
   void updateBlockHeadersProgress(float progress, unsigned secondsRem);
   void updateBlockDataProgress(float progress, unsigned secondsRem);
   void updateRescanProgress(float progress, unsigned secondsRem);

private:
   void setupBtcIcon(NetworkType);
   void SetLoggedinStatus();
   void SetLoggedOutStatus();
   void SetCelerConnectingStatus();
   QWidget *CreateSeparator();
   [[deprecated]] void setBalances();
   void displayBalances();
   void updateConnectionStatusDetails(ArmoryState state, unsigned int blockNum);

private:
   void updateProgress(float progress, unsigned secondsRem);
   [[deprecated]] QString getImportingText() const;

   bool eventFilter(QObject *object, QEvent *event) override;

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
   std::vector<std::string>   balanceSymbols_;
   std::unordered_map<std::string, double>   balances_;
   std::unordered_map<std::string, BTCNumericTypes::balance_type> xbtBalances_;
   int            armoryState_{ -1 };
   unsigned int   blockNum_{ 0 };

   std::chrono::steady_clock::time_point timeSinceLastBlock_{std::chrono::steady_clock::now()};
};

#endif // __STATUS_BAR_VIEW_H__
