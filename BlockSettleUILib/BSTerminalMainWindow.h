/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __BS_TERMINAL_MAIN_WINDOW_H__
#define __BS_TERMINAL_MAIN_WINDOW_H__

#include <QMainWindow>
#include <QStandardItemModel>

#include <memory>
#include <vector>

#include "ApplicationSettings.h"
#include "ArmoryObject.h"
#include "BsClient.h"
#include "CelerClientProxy.h"
#include "QWalletInfo.h"
#include "SignContainer.h"
#include "WalletSignerContainer.h"
#include "ZMQ_BIP15X_DataConnection.h"
#include "ZMQ_BIP15X_Helpers.h"

#include "ChatProtocol/ChatClientService.h"

namespace Ui {
    class BSTerminalMainWindow;
}
namespace bs {
   class LogManager;
   namespace sync {
      class Wallet;
      class WalletsManager;
   }
}

struct NetworkSettings;

class AboutDialog;
class ArmoryServersProvider;
class AssetManager;
class AuthAddressDialog;
class AuthAddressManager;
class AutheIDClient;
class AutoSignQuoteProvider;
class BSMarketDataProvider;
class BSTerminalSplashScreen;
class BaseCelerClient;
class CCFileManager;
class CCPortfolioModel;
class ConnectionManager;
class LoginWindow;
class NetworkSettingsLoader;
class OrderListModel;
class QSystemTrayIcon;
class RequestReplyCommand;
class SignersProvider;
class StatusBarView;
class StatusViewBlockListener;
class TransactionsViewModel;
class WalletManagementWizard;

class BSTerminalMainWindow : public QMainWindow
{
Q_OBJECT

public:
   BSTerminalMainWindow(const std::shared_ptr<ApplicationSettings>& settings
      , BSTerminalSplashScreen& splashScreen, QWidget* parent = nullptr);
   ~BSTerminalMainWindow() override;

   void postSplashscreenActions();

   bool event(QEvent *event) override;
   void addDeferredDialog(const std::function<void(void)> &deferredDialog);

private:
   void setupToolbar();
   void setupMenu();
   void setupIcon();

   void setupWalletsView();
   void setupTransactionsView();

   void initConnections();
   void initArmory();
   void connectArmory();
   void connectSigner();
   std::shared_ptr<WalletSignerContainer> createSigner();
   std::shared_ptr<WalletSignerContainer> createRemoteSigner(bool restoreHeadless = false);
   std::shared_ptr<WalletSignerContainer> createLocalSigner();

   void setTabStyle();

   void LoadWallets();
   void InitAuthManager();
   bool InitSigningContainer();
   void InitAssets();

   void InitPortfolioView();
   void InitWalletsView();
   void InitChartsView();

   void tryInitChatView();
   void tryLoginIntoChat();
   void tryGetChatKeys();

   void UpdateMainWindowAppearence();

   bool isMDLicenseAccepted() const;
   void saveUserAcceptedMDLicense();

   bool showStartupDialog();
   void LoadCCDefinitionsFromPuB();
   void setWidgetsAuthorized(bool authorized);

signals:
   void armoryServerPromptResultReady();

private slots:
   void InitTransactionsView();
   void ArmoryIsOffline();
   void SignerReady();
   void showInfo(const QString &title, const QString &text);
   void showError(const QString &title, const QString &text);
   void onSignerConnError(SignContainer::ConnectionError error, const QString &details);

   void CompleteUIOnlineView();
   void CompleteDBConnection();

   bool createWallet(bool primary, const std::function<void()> & = nullptr, bool reportSuccess = true);
   void onCreatePrimaryWalletRequest();

   void acceptMDAgreement();
   void updateControlEnabledState();
   void onButtonUserClicked();
   void showArmoryServerPrompt(const BinaryData& srvPubKey, const std::string& srvIPPort, std::shared_ptr<std::promise<bool> > promiseObj);

   void onArmoryNeedsReconnect();
   void onCCLoaded();

   void onTabWidgetCurrentChanged(const int &index);

private:
   std::unique_ptr<Ui::BSTerminalMainWindow> ui_;
   QAction *action_send_ = nullptr;
   QAction *action_generate_address_ = nullptr;
   QAction *action_login_ = nullptr;
   QAction *action_logout_ = nullptr;

   std::shared_ptr<bs::LogManager>        logMgr_;
   std::shared_ptr<ApplicationSettings>   applicationSettings_;
   std::shared_ptr<bs::sync::WalletsManager> walletsMgr_;
   std::shared_ptr<ArmoryServersProvider> armoryServersProvider_;
   std::shared_ptr<SignersProvider>       signersProvider_;
   std::shared_ptr<AuthAddressManager>    authManager_;
   std::shared_ptr<ArmoryObject>          armory_;

   std::shared_ptr<StatusBarView>            statusBarView_;
   std::shared_ptr<QSystemTrayIcon>          sysTrayIcon_;
   std::shared_ptr<TransactionsViewModel>    transactionsModel_;
   std::shared_ptr<CCPortfolioModel>         portfolioModel_;
   std::shared_ptr<ConnectionManager>        connectionManager_;
   std::shared_ptr<CelerClientProxy>         celerConnection_;
   std::shared_ptr<BSMarketDataProvider>     mdProvider_;
   std::shared_ptr<AssetManager>             assetManager_;
   std::shared_ptr<CCFileManager>            ccFileManager_;
   std::shared_ptr<AuthAddressDialog>        authAddrDlg_;
   std::shared_ptr<WalletSignerContainer>    signContainer_;
   std::shared_ptr<AutoSignQuoteProvider>    autoSignQuoteProvider_;

   std::unique_ptr<OrderListModel>           orderListModel_;

   std::shared_ptr<WalletManagementWizard> walletsWizard_;

   QString currentUserLogin_;

public slots:
   void onReactivate();
   void raiseWindow();

private:
   struct TxInfo;

private slots:
   void onSend();
   void onGenerateAddress();

   void openAuthManagerDialog();
   void openAuthDlgVerify(const QString &addrToVerify);
   void openConfigDialog();
   void openAccountInfoDialog();
   void openCCTokenDialog();

   void onZCreceived(const std::vector<bs::TXEntry> &);
   void showZcNotification(const TxInfo &);

   void onLogin();
   void onLogout();

   void onCelerConnected();
   void onCelerDisconnected();
   void onCelerConnectionError(int errorCode);
   void showRunInBackgroundMessage();
   void onCCInfoMissing();

   void onMDConnectionDetailsRequired();

   void onBsConnectionFailed();

protected:
   void closeEvent(QCloseEvent* event) override;
   void changeEvent(QEvent* e) override;

private:
   void onUserLoggedIn();
   void onUserLoggedOut();

   void setLoginButtonText(const QString& text);

   void setupShortcuts();

   void createAdvancedTxDialog(const std::string &selectedWalletId);
   void createAuthWallet(const std::function<void()> &);

   bool isUserLoggedIn() const;
   bool isArmoryConnected() const;

   void InitWidgets();

   void networkSettingsReceived(const NetworkSettings &settings);

private:
   enum class ChatInitState
   {
      NoStarted,
      InProgress,
      Done,
   };

   QString           loginButtonText_;

   bool initialWalletCreateDialogShown_ = false;
   bool deferCCsync_ = false;

   bool wasWalletsRegistered_ = false;
   bool walletsSynched_ = false;
   bool isArmoryReady_ = false;

   std::unique_ptr<NetworkSettingsLoader> networkSettingsLoader_;

   SignContainer::ConnectionError lastSignerError_{};

   ZmqBipNewKeyCb   cbApprovePuB_ = nullptr;
   ZmqBipNewKeyCb   cbApproveChat_ = nullptr;
   ZmqBipNewKeyCb   cbApproveProxy_ = nullptr;

   std::queue<std::function<void(void)>> deferredDialogs_;
   bool deferredDialogRunning_ = false;

   class MainWinACT : public ArmoryCallbackTarget
   {
   public:
      MainWinACT(BSTerminalMainWindow *wnd)
         : parent_(wnd) {}
      ~MainWinACT() override { cleanup(); }
      void onZCReceived(const std::vector<bs::TXEntry> &) override;
      void onStateChanged(ArmoryState) override;
      void onTxBroadcastError(const std::string &hash, const std::string &error) override;
      void onRefresh(const std::vector<BinaryData> &, bool) override;

   private:
      BSTerminalMainWindow *parent_;
   };
   std::unique_ptr<MainWinACT>   act_;

   std::unique_ptr<BsClient> bsClient_;

   Chat::ChatClientServicePtr chatClientServicePtr_;

   ChatInitState chatInitState_{ChatInitState::NoStarted};
   bool networkSettingsReceived_{false};
   bool gotChatKeys_{false};
   BinaryData chatTokenData_;
   SecureBinaryData chatTokenSign_;
   BinaryData chatPubKey_;
   SecureBinaryData chatPrivKey_;

};

#endif // __BS_TERMINAL_MAIN_WINDOW_H__
