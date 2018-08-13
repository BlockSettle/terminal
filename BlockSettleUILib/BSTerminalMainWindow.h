#ifndef __BS_TERMINAL_MAIN_WINDOW_H__
#define __BS_TERMINAL_MAIN_WINDOW_H__

#include <QMainWindow>
#include <QStandardItemModel>

#include <memory>
#include <vector>

#include "ApplicationSettings.h"
#include "ArmoryConnection.h"
#include "CelerClient.h"
#include "TransactionsViewModel.h"

namespace Ui {
    class BSTerminalMainWindow;
}
namespace bs {
   class LogManager;
}

class AboutDialog;
class AssetManager;
class AuthAddressDialog;
class AuthAddressManager;
class BSTerminalSplashScreen;
class CCFileManager;
class CCPortfolioModel;
class CelerClient;
class ConnectionManager;
class HeadlessAddressSyncer;
class MarketDataProvider;
class OTPManager;
class QSystemTrayIcon;
class SignContainer;
class StatusBarView;
class StatusViewBlockListener;
class WalletManagementWizard;
class WalletsManager;

class BSTerminalMainWindow : public QMainWindow
{
Q_OBJECT

public:
   BSTerminalMainWindow(const std::shared_ptr<ApplicationSettings>& settings, BSTerminalSplashScreen& splashScreen, QWidget* parent = nullptr);
   ~BSTerminalMainWindow() override;

private:
   void setupToolbar();
   void setupMenu();
   void setupIcon();

   void setupWalletsView();
   void setupTransactionsView();

   void InitConnections();
   void initArmory();
   void connectArmory();

   void setTabStyle();

   void LoadWallets(BSTerminalSplashScreen& splashScreen);
   void InitAuthManager();
   void InitSigningContainer();
   void InitAssets();

   void InitPortfolioView();
   void InitWalletsView();

   void CompleteUIOnlineView();
   void CompleteDBConnection();

   void InitOTP();

   void UpdateMainWindowAppearence();

private slots:
   void InitTransactionsView();
   void SetOfflineUIView();
   void SignerReady();
   void onPasswordRequested(std::string walletId, std::string prompt
      , std::vector<bs::wallet::EncryptionType>, std::vector<SecureBinaryData> encKeys
      , bs::wallet::KeyRank);
   void showInfo(const QString &title, const QString &text);
   void showError(const QString &title, const QString &text);

   void OnOTPSyncCompleted();

private:
   QAction *action_send_;
   QAction *action_receive_;
   QAction *action_login_;
   QAction *action_logout_;

private:
   Ui::BSTerminalMainWindow* ui;

   std::shared_ptr<bs::LogManager>        logMgr_;
   std::shared_ptr<ApplicationSettings>   applicationSettings_;
   std::shared_ptr<WalletsManager>        walletsManager_;
   std::shared_ptr<AuthAddressManager>    authManager_;
   std::shared_ptr<ArmoryConnection>      armory_;

   std::shared_ptr<StatusBarView>         statusBarView_;
   std::shared_ptr<QSystemTrayIcon>       sysTrayIcon_;
   std::shared_ptr<TransactionsViewModel> transactionsModel_;
   std::shared_ptr<CCPortfolioModel>      portfolioModel_;
   std::shared_ptr<ConnectionManager>     connectionManager_;
   std::shared_ptr<CelerClient>           celerConnection_;
   std::shared_ptr<MarketDataProvider>    mdProvider_;
   std::shared_ptr<AssetManager>          assetManager_;
   std::shared_ptr<OTPManager>            otpManager_;
   std::shared_ptr<CCFileManager>         ccFileManager_;
   std::shared_ptr<AuthAddressDialog>     authAddrDlg_;
   std::shared_ptr<AboutDialog>           aboutDlg_;
   std::shared_ptr<SignContainer>         signContainer_;
   std::shared_ptr<HeadlessAddressSyncer> addrSyncer_;

   std::shared_ptr<WalletManagementWizard> walletsWizard_;

   bool  widgetsInited_ = false;

   struct TxInfo {
      Tx       tx;
      uint32_t txTime;
      int64_t  value;
      std::shared_ptr<bs::Wallet>   wallet;
      bs::Transaction::Direction    direction;
      QString  mainAddress;
   };

public slots:
   void onReactivate();

private slots:
   void onSend();
   void onReceive();

   void openAuthManagerDialog();
   void openAuthDlgVerify(const QString &addrToVerify);
   void openConfigDialog();
   void openAccountInfoDialog();
   void openOTPDialog();
   void openCCTokenDialog();
   void showZcNotification(const TxInfo &);
   void onZCreceived(ArmoryConnection::ReqIdType);
   void onArmoryStateChanged(ArmoryConnection::State);

   void onLogin();
   void onLogout();

   void onCelerConnected();
   void onCelerDisconnected();
   void onCelerConnectionError(int errorCode);
   void showRunInBackgroundMessage();
   void onAuthMgrConnComplete();
   void onCCInfoMissing();

protected:
   void closeEvent(QCloseEvent* event) override;
   void changeEvent(QEvent* e) override;

private:
   void onUserLoggedIn();
   void onUserLoggedOut();

   bool createWallet(bool primary, bool reportSuccess = true);
   void setLoginButtonText(const QString& text);

   void setupShortcuts();

   void createAdvancedTxDialog(const std::string &selectedWalletId);
};

Q_DECLARE_METATYPE(std::string)
Q_DECLARE_METATYPE(std::vector<UTXO>)

#endif // __BS_TERMINAL_MAIN_WINDOW_H__
