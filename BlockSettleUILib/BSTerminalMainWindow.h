/*

***********************************************************************************
* Copyright (C) 2018 - 2020, BlockSettle AB
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
#include "BIP15xHelpers.h"

#include "ChatProtocol/ChatClientService.h"

namespace Ui {
    class BSTerminalMainWindow;
}
namespace bs {
   class LogManager;
   class UTXOReservationManager;
   struct TradeSettings;
   namespace sync {
      class Wallet;
      class WalletsManager;
   }
}

namespace Blocksettle {
   namespace Communication {
      namespace ProxyTerminalPb {
         class Response;
      }
   }
}

class QLockFile;

struct BsClientLoginResult;
struct NetworkSettings;

class AboutDialog;
class ArmoryServersProvider;
class AssetManager;
class AuthAddressDialog;
class AuthAddressManager;
class AutheIDClient;
class AutoSignScriptProvider;
class BSMarketDataProvider;
class BSTerminalSplashScreen;
class BaseCelerClient;
class BootstrapDataManager;
class CCFileManager;
class CCPortfolioModel;
class CcTrackerClient;
class ConnectionManager;
class CreateTransactionDialog;
class LoginWindow;
class MDCallbacksQt;
class OrderListModel;
class QSystemTrayIcon;
class RequestReplyCommand;
class SignersProvider;
class StatusBarView;
class StatusViewBlockListener;
class TransactionsViewModel;
class WalletManagementWizard;

enum class BootstrapFileError: int;

class BSTerminalMainWindow : public QMainWindow
{
Q_OBJECT

public:
   BSTerminalMainWindow(const std::shared_ptr<ApplicationSettings>& settings
      , BSTerminalSplashScreen& splashScreen, QLockFile &lockFile, QWidget* parent = nullptr);
   ~BSTerminalMainWindow() override;

   void postSplashscreenActions();
   void loadPositionAndShow();

   bool event(QEvent *event) override;
   void addDeferredDialog(const std::function<void(void)> &deferredDialog);

private:
   void setupToolbar();
   void setupTopRightWidget();
   void setupMenu();
   void setupIcon();

   void setupWalletsView();
   void setupTransactionsView();
   void setupInfoWidget();

   void initConnections();
   void initArmory();
   void initCcClient();
   void initUtxoReservationManager();
   void initBootstrapDataManager();
   void connectArmory();
   void connectCcClient();
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
   void resetChatKeys();
   void tryGetChatKeys();

   void UpdateMainWindowAppearence();

   bool isMDLicenseAccepted() const;
   void saveUserAcceptedMDLicense();

   bool showStartupDialog();
   void setWidgetsAuthorized(bool authorized);

   void openURIDialog();

signals:
   void armoryServerPromptResultReady();

private slots:
   void InitTransactionsView();
   void ArmoryIsOffline();
   void SignerReady();
   void onNeedNewWallet();
   void showInfo(const QString &title, const QString &text);
   void showError(const QString &title, const QString &text);
   void onSignerConnError(SignContainer::ConnectionError error, const QString &details);

   void CompleteUIOnlineView();
   void CompleteDBConnection();

   bool createPrimaryWallet();
   void onCreatePrimaryWalletRequest();

   void acceptMDAgreement();
   void updateControlEnabledState();
   void onButtonUserClicked();
   void showArmoryServerPrompt(const BinaryData& srvPubKey, const std::string& srvIPPort, std::shared_ptr<std::promise<bool> > promiseObj);

   void onArmoryNeedsReconnect();
   void onCCLoaded();

   void onTabWidgetCurrentChanged(const int &index);
   void onSyncWallets();
   void onSignerVisibleChanged();

   void onAuthLeafCreated();

   void onDeliverFutureObligations(const QModelIndex& index);

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
   std::shared_ptr<CcTrackerClient>       trackerClient_;

   std::shared_ptr<StatusBarView>            statusBarView_;
   std::shared_ptr<QSystemTrayIcon>          sysTrayIcon_;
   std::shared_ptr<TransactionsViewModel>    transactionsModel_;
   std::shared_ptr<CCPortfolioModel>         portfolioModel_;
   std::shared_ptr<ConnectionManager>        connectionManager_;
   std::shared_ptr<CelerClientProxy>         celerConnection_;
   std::shared_ptr<BSMarketDataProvider>     mdProvider_;
   std::shared_ptr<MDCallbacksQt>            mdCallbacks_;
   std::shared_ptr<AssetManager>             assetManager_;
   std::shared_ptr<CCFileManager>            ccFileManager_;
   std::shared_ptr<BootstrapDataManager>     bootstrapDataManager_;
   std::shared_ptr<AuthAddressDialog>        authAddrDlg_;
   std::shared_ptr<WalletSignerContainer>    signContainer_;
   std::shared_ptr<AutoSignScriptProvider>   autoSignQuoteProvider_;
   std::shared_ptr<AutoSignScriptProvider>   autoSignRFQProvider_;

   std::shared_ptr<OrderListModel>           orderListModel_;

   std::shared_ptr<WalletManagementWizard> walletsWizard_;
   std::shared_ptr<bs::UTXOReservationManager> utxoReservationMgr_{};

   QString currentUserLogin_;


   unsigned armoryReconnectDelay_ = 0;
   std::chrono::time_point<std::chrono::steady_clock> nextArmoryReconnectAttempt_;

public slots:
   void onReactivate();
   void raiseWindow();

private:
   struct TxInfo;

   enum class AutoLoginState
   {
      Idle,
      Connecting,
      Connected,
      Failed,
   };

private slots:

   void onSend();
   void onGenerateAddress();

   void openAuthManagerDialog();
   void openConfigDialog(bool showInNetworkPage = false);
   void openAccountInfoDialog();
   void openCCTokenDialog();

   void onZCreceived(const std::vector<bs::TXEntry> &);
   void showZcNotification(const TxInfo &);
   void onNodeStatus(NodeStatus, bool isSegWitEnabled, RpcStatus);

   void onLogin();
   void onLogout();

   void onCelerConnected();
   void onCelerDisconnected();
   void onCelerConnectionError(int errorCode);
   void showRunInBackgroundMessage();

   void onBsConnectionDisconnected();
   void onBsConnectionFailed();

   void onInitWalletDialogWasShown();

protected:
   void closeEvent(QCloseEvent* event) override;
   void changeEvent(QEvent* e) override;

private:
   void onUserLoggedIn();
   void onUserLoggedOut();

   void onAccountTypeChanged(bs::network::UserType userType, bool enabled);

   void setLoginButtonText(const QString& text);

   void setupShortcuts();

   bool isUserLoggedIn() const;
   bool isArmoryConnected() const;

   void InitWidgets();

   void enableTradingIfNeeded();

   void showLegacyWarningIfNeeded();

   void promptSwitchEnv(bool prod);
   void switchToTestEnv();
   void switchToProdEnv();

   void restartTerminal();
   void processDeferredDialogs();

   std::shared_ptr<BsClient> createClient();
   void activateClient(const std::shared_ptr<BsClient> &bsClient
      , const BsClientLoginResult &result, const std::string &email);
   const std::string &loginApiKeyEncrypted() const;
   void initApiKeyLogins();
   void tryLoginUsingApiKey();

   void DisplayCreateTransactionDialog(std::shared_ptr<CreateTransactionDialog> dlg);

   void onBootstrapDataLoaded(const std::string& data);

   void SendBSDeliveryAddress();

private:
   enum class ChatInitState
   {
      NoStarted,
      InProgress,
      Done,
   };

   QString           loginButtonText_;
   AutoLoginState    autoLoginState_{AutoLoginState::Idle};
   QString autoLoginLastErrorMsg_;
   std::string loginApiKeyEncrypted_;
   QTimer *loginTimer_{};
   std::shared_ptr<BsClient> autoLoginClient_;

   bool initialWalletCreateDialogShown_ = false;
   bool deferCCsync_ = false;

   bool wasWalletsRegistered_ = false;
   bool walletsSynched_ = false;
   bool isArmoryReady_ = false;

   SignContainer::ConnectionError lastSignerError_{};

   bs::network::BIP15xNewKeyCb   cbApproveChat_{ nullptr };
   bs::network::BIP15xNewKeyCb   cbApproveProxy_{ nullptr };
   bs::network::BIP15xNewKeyCb   cbApproveCcServer_{ nullptr };
   bs::network::BIP15xNewKeyCb   cbApproveExtConn_{ nullptr };

   std::queue<std::function<void(void)>> deferredDialogs_;
   bool deferredDialogRunning_ = false;

   uint32_t armoryRestartCount_{};

   class MainWinACT : public ArmoryCallbackTarget
   {
   public:
      MainWinACT(BSTerminalMainWindow *wnd)
         : parent_(wnd) {}
      ~MainWinACT() override { cleanup(); }
      void onZCReceived(const std::string& requestId, const std::vector<bs::TXEntry>&) override;
      void onStateChanged(ArmoryState) override;
      void onTxBroadcastError(const std::string& requestId, const BinaryData &txHash, int errCode
         , const std::string &errMsg) override;
      void onNodeStatus(NodeStatus, bool isSegWitEnabled, RpcStatus) override;

   private:
      BSTerminalMainWindow *parent_;
   };
   std::unique_ptr<MainWinACT>   act_;

   std::shared_ptr<BsClient> bsClient_;

   Chat::ChatClientServicePtr chatClientServicePtr_;

   ChatInitState chatInitState_{ChatInitState::NoStarted};
   bool gotChatKeys_{false};
   BinaryData chatTokenData_;
   SecureBinaryData chatTokenSign_;
   BinaryData chatPubKey_;
   SecureBinaryData chatPrivKey_;

   // Default is online to not show online notification after terminal startup
   bool isBitcoinCoreOnline_{true};

   bool accountEnabled_{true};

   QLockFile &lockFile_;

   bs::network::UserType userType_{};

   std::shared_ptr<bs::TradeSettings> tradeSettings_;
};

#endif // __BS_TERMINAL_MAIN_WINDOW_H__
