#ifndef __STATUS_BAR_VIEW_H__
#define __STATUS_BAR_VIEW_H__

#include <QObject>
#include <QStatusBar>
#include <QLabel>
#include <QPixmap>
#include <QIcon>

#include <memory>

#include "CelerClient.h"
#include "CircleProgressBar.h"


class AssetManager;
class PyBlockDataManager;
class SignContainer;
class StatusViewBlockListener;
class WalletsManager;

class StatusBarView  : public QObject
{
   Q_OBJECT

signals:
   void onSetOfflineStatus();
   void onSetConnectingStatus();
   void onSetErrorStatus(const QString&);
   void onSetInitializingCompleteStatus();

   void onUpdateDBHeadersProgress(float progress, unsigned secondsRem);
   void onUpdateOrganizingChainProgress(float progress, unsigned secondsRem);
   void onUpdateBlockHeadersProgress(float progress, unsigned secondsRem);
   void onUpdateBlockDataProgress(float progress, unsigned secondsRem);
   void onUpdateRescanProgress(float progress, unsigned secondsRem);

public:
   StatusBarView(const std::shared_ptr<PyBlockDataManager>& bdm, std::shared_ptr<WalletsManager> walletsManager, std::shared_ptr<AssetManager> assetManager, QStatusBar *parent = nullptr);
   ~StatusBarView() noexcept override = default;

   StatusBarView(const StatusBarView&) = delete;
   StatusBarView& operator = (const StatusBarView&) = delete;

   StatusBarView(StatusBarView&&) = delete;
   StatusBarView& operator = (StatusBarView&&) = delete;

   void connectToCelerClient(const std::shared_ptr<CelerClient>& celerClient);
   void connectToContainer(const std::shared_ptr<SignContainer> &);

public slots:
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
   void setConnectingStatus();
   void setOfflineStatus();
   void setInitializingCompleteStatus();
   void setErrorStatus(const QString& errorMessage );

   void updateDBHeadersProgress(float progress, unsigned secondsRem);
   void updateOrganizingChainProgress(float progress, unsigned secondsRem);
   void updateBlockHeadersProgress(float progress, unsigned secondsRem);
   void updateBlockDataProgress(float progress, unsigned secondsRem);
   void updateRescanProgress(float progress, unsigned secondsRem);

private:
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

   QLabel            *estimateLabel_;
   QLabel            *balanceLabel_;
   QLabel            *celerConnectionIconLabel_;
   QLabel            *connectionStatusLabel_;
   QLabel            *containerStatusLabel_;
   CircleProgressBar *progressBar_;

   QIcon      iconCeler_;
   QPixmap    iconOffline_;
   QPixmap    iconError_;
   QPixmap    iconOnline_;
   QPixmap    iconConnecting_;
   QPixmap    iconCelerOffline_;
   QPixmap    iconCelerError_;
   QPixmap    iconCelerOnline_;
   QPixmap    iconCelerConnecting_;
   QPixmap    iconContainerOffline_;
   QPixmap    iconContainerError_;
   QPixmap    iconContainerConnecting_;
   QPixmap    iconContainerOnline_;

   std::shared_ptr<PyBlockDataManager> bdm_;
   std::shared_ptr<WalletsManager> walletsManager_;
   std::shared_ptr<StatusViewBlockListener> listener_;
   std::shared_ptr<AssetManager> assetManager_;
   std::unordered_set<std::string>  importingWallets_;
};

#endif // __STATUS_BAR_VIEW_H__
