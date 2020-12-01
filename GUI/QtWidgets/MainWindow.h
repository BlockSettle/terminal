/*

***********************************************************************************
* Copyright (C) 2018 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __GUI_QT_MAIN_WINDOW_H__
#define __GUI_QT_MAIN_WINDOW_H__

#include <functional>
#include <queue>
#include <QMainWindow>
#include "Address.h"
#include "ArmoryConnection.h"
#include "AuthAddress.h"
#include "BsClient.h"
#include "Celer/BaseCelerClient.h"
#include "CommonTypes.h"
#include "SignContainer.h"
#include "Settings/SignersProvider.h"
#include "UiUtils.h"

namespace spdlog {
   class logger;
}
namespace Ui {
    class BSTerminalMainWindow;
}
namespace bs {
   namespace message {
      class QueueInterface;
      class User;
   }
}

class AboutDialog;
class AuthAddressDialog;
class ConfigDialog;
class CreateTransactionDialog;
class DialogManager;
class LoginWindow;
class NotificationCenter;
class OrderListModel;
class QSystemTrayIcon;
class StatusBarView;
class TransactionsViewModel;

namespace bs {
   namespace gui {
      namespace qt {
         class MainWindow : public QMainWindow
         {
         Q_OBJECT

         public:
            MainWindow(const std::shared_ptr<spdlog::logger> &
               , const std::shared_ptr<bs::message::QueueInterface> &
               , const std::shared_ptr < bs::message::User> &);
            ~MainWindow() override;

            void onSetting(int setting, const QVariant &value);
            void onSettingsState(const ApplicationSettings::State&);
            void onGetGeometry(const QRect &);
            void showStartupDialog(bool showLic);

            void onArmoryStateChanged(int state, unsigned int blockNum);
            void onNewBlock(int state, unsigned int blockNum);
            void onSignerStateChanged(int state, const std::string &);
            void onWalletsReady();

            void onHDWallet(const bs::sync::WalletInfo &);
            void onHDWalletDetails(const bs::sync::HDWalletData &);
            void onWalletsList(const std::string &id, const std::vector<bs::sync::HDWalletData>&);
            void onWalletData(const std::string &walletId, const bs::sync::WalletData&);
            void onAddresses(const std::vector<bs::sync::Address> &);
            void onAddressComments(const std::string &walletId
               , const std::map<bs::Address, std::string> &);
            void onWalletBalance(const bs::sync::WalletBalanceData &);
            void onLedgerEntries(const std::string &filter, uint32_t totalPages
               , uint32_t curPage, uint32_t curBlock, const std::vector<bs::TXEntry> &);
            void onTXDetails(const std::vector<bs::sync::TXWalletDetails> &);
            void onNewZCs(const std::vector<bs::sync::TXWalletDetails>&);
            void onZCsInvalidated(const std::vector<BinaryData>& txHashes);
            void onAddressHistory(const bs::Address&, uint32_t curBlock
               , const std::vector<bs::TXEntry>&);

            void onFeeLevels(const std::map<unsigned int, float>&);
            void onUTXOs(const std::string& id, const std::string& walletId, const std::vector<UTXO>&);
            void onSignedTX(const std::string &id, BinaryData signedTX, bs::error::ErrorCode result);
            void onArmoryServers(const QList<ArmoryServer>&, int idxCur, int idxConn);
            void onSignerSettings(const QList<SignerHost>&, const std::string& ownKey, int idxCur);

            void onLoginStarted(const std::string &login, bool success, const std::string &errMsg);
            void onLoggedIn(const BsClientLoginResult&);
            void onAccountTypeChanged(bs::network::UserType userType, bool enabled);
            void onMatchingLogin(const std::string& mtchLogin, BaseCelerClient::CelerUserType
               , const std::string &userId);
            void onMatchingLogout();

            void onNewSecurity(const std::string& name, bs::network::Asset::Type);
            void onMDUpdated(bs::network::Asset::Type assetType
               , const QString& security, const bs::network::MDFields &);
            void onBalance(const std::string& currency, double balance);

            void onAuthAddresses(const std::vector<bs::Address>&
               , const std::map<bs::Address, AddressVerificationState> &);
            void onSubmittedAuthAddresses(const std::vector<bs::Address>&);
            void onVerifiedAuthAddresses(const std::vector<bs::Address>&);
            void onAuthKey(const bs::Address&, const BinaryData& authKey);

            void onQuoteReceived(const bs::network::Quote&);
            void onQuoteMatched(const std::string &rfqId, const std::string &quoteId);
            void onQuoteFailed(const std::string& rfqId, const std::string& quoteId
               , const std::string &info);
            void onSettlementPending(const std::string& rfqId, const std::string& quoteId
               , const BinaryData& settlementId, int timeLeftMS);
            void onSettlementComplete(const std::string& rfqId, const std::string& quoteId
               , const BinaryData& settlementId);
            void onQuoteReqNotification(const bs::network::QuoteReqNotification&);
            void onOrdersUpdate(const std::vector<bs::network::Order>&);
            void onQuoteCancelled(const std::string& rfqId, const std::string& quoteId
               , bool byUser);

            void onReservedUTXOs(const std::string& resId, const std::string& subId
               , const std::vector<UTXO>&);

         public slots:
            void onReactivate();
            void raiseWindow();

/*         private:
            enum class AutoLoginState
            {
               Idle,
               Connecting,
               Connected,
            };*/

         signals:
            void getSettings(const std::vector<ApplicationSettings::Setting> &);
            void putSetting(ApplicationSettings::Setting, const QVariant &);
            void resetSettings(const std::vector<ApplicationSettings::Setting> &);
            void resetSettingsToState(const ApplicationSettings::State&);
            void needSettingsState();
            void needArmoryServers();
            void setArmoryServer(int);
            void addArmoryServer(const ArmoryServer&);
            void delArmoryServer(int);
            void updArmoryServer(int, const ArmoryServer&);
            void needArmoryReconnect();
            void needSigners();
            void setSigner(int);
            void createNewWallet();
            void needHDWalletDetails(const std::string &walletId);
            void needWalletsList(UiUtils::WalletsTypes, const std::string &id);
            void needWalletBalances(const std::string &walletId);
            void needWalletData(const std::string& walletId);

            void needExtAddresses(const std::string &walletId);
            void needIntAddresses(const std::string &walletId);
            void needUsedAddresses(const std::string &walletId);
            void needAddrComments(const std::string &walletId, const std::vector<bs::Address> &);
            void setAddrComment(const std::string &walletId, const bs::Address &
               , const std::string &comment);

            void needLedgerEntries(const std::string &filter);
            void needTXDetails(const std::vector<bs::sync::TXWallet>&
               , bool useCache=true, const bs::Address& addr = {});
            void needAddressHistory(const bs::Address&);

            void needFeeLevels(const std::vector<unsigned int>&);
            void needUTXOs(const std::string& id, const std::string& walletId
               , bool confOnly = false, bool swOnly = false);

            void needSignTX(const std::string& id, const bs::core::wallet::TXSignRequest&
               , bool keepDupRecips = false, SignContainer::TXSignMode mode = SignContainer::TXSignMode::Full);
            void needBroadcastZC(const std::string& id, const BinaryData&);
            void needSetTxComment(const std::string& walletId, const BinaryData& txHash, const std::string& comment);

            void needOpenBsConnection();
            void needCloseBsConnection();
            void needStartLogin(const std::string& login);
            void needCancelLogin();
            void needMatchingLogout();
            void needMdConnection(ApplicationSettings::EnvConfiguration);

            void needNewAuthAddress();
            void needSubmitAuthAddress(const bs::Address&);
            void needSubmitRFQ(const bs::network::RFQ&, const std::string& reserveId = {});
            void needAcceptRFQ(const std::string& id, const bs::network::Quote&);
            void needCancelRFQ(const std::string& id);
            void needAuthKey(const bs::Address&);
            void needReserveUTXOs(const std::string& reserveId, const std::string& subId
               , uint64_t amount, bool partial = false, const std::vector<UTXO>& utxos = {});
            void needUnreserveUTXOs(const std::string& reserveId, const std::string& subId);

            void submitQuote(const bs::network::QuoteNotification&);
            void pullQuote(const std::string& settlementId, const std::string& reqId
               , const std::string& reqSessToken);

         private slots:
            void onSend();
            void onGenerateAddress();

            void openAuthManagerDialog();
            void openConfigDialog(bool showInNetworkPage = false);
         //   void openAccountInfoDialog();
         //   void openCCTokenDialog();

            void onLoginInitiated();
            void onLogoutInitiated();
            void onLoggedOut();
            void onButtonUserClicked();

//            void onCelerConnected();
//            void onCelerDisconnected();
//            void onCelerConnectionError(int errorCode);
            void showRunInBackgroundMessage();

//            void onBsConnectionDisconnected();
//            void onBsConnectionFailed();

            void onSignerVisibleChanged();

         protected:
            bool event(QEvent *) override;
            void closeEvent(QCloseEvent *) override;
            void changeEvent(QEvent *) override;

         private:
            void setLoginButtonText(const QString& text);

            void setupShortcuts();
            void setupInfoWidget();
            void setupIcon();
            void setupToolbar();
            void setupMenu();
            void setupTopRightWidget();

            void updateAppearance();
            void setWidgetsAuthorized(bool);

            void initWidgets();
            void initTransactionsView();
            void initChartsView();

            void promptSwitchEnv(bool prod);
            void switchToTestEnv();
            void switchToProdEnv();

            void restartTerminal();
            void addDeferredDialog(const std::function<void(void)> &);
            void processDeferredDialogs();

            void activateClient(const BsClientLoginResult&);

         private:
            std::unique_ptr<Ui::BSTerminalMainWindow> ui_;
            std::shared_ptr<spdlog::logger>  logger_;
            std::shared_ptr<bs::message::QueueInterface> queue_;
            std::shared_ptr<bs::message::User>  guiUser_, settingsUser_;

            QAction *actSend_{ nullptr };
            QAction *actNewAddress_{ nullptr };
            QAction *actLogin_{ nullptr };
            QAction *actLogout_{ nullptr };

            std::shared_ptr<StatusBarView>            statusBarView_;
            std::shared_ptr<QSystemTrayIcon>          sysTrayIcon_;
            std::shared_ptr<NotificationCenter>       notifCenter_;
            //   std::shared_ptr<CCPortfolioModel>         portfolioModel_;

            std::shared_ptr<TransactionsViewModel>    txModel_;
            std::shared_ptr<OrderListModel>           orderListModel_;

            std::shared_ptr<DialogManager>   dialogMgr_;
            CreateTransactionDialog* txDlg_{ nullptr };
            ConfigDialog*  cfgDlg_{ nullptr };
            LoginWindow* loginDlg_{ nullptr };
            AuthAddressDialog* authAddrDlg_{ nullptr };

            //   std::shared_ptr<WalletManagementWizard> walletsWizard_;

            bool     accountEnabled_{ false };
            QString  currentUserLogin_;
            QString  loginButtonText_;
            QTimer * loginTimer_{};

            bool closeToTray_{ false };
            bool initialWalletCreateDialogShown_ = false;
            bool deferCCsync_ = false;
            bool advTxDlgByDefault_{ false };
            ApplicationSettings::EnvConfiguration envConfig_{ ApplicationSettings::EnvConfiguration::Unknown };

            std::queue<std::function<void(void)>> deferredDialogs_;
            bool deferredDialogRunning_ = false;
            //   bs::network::UserType userType_{};

            uint32_t topBlock_{ 0 };
         };
      }
   }
} // namespaces

#endif // __GUI_QT_MAIN_WINDOW_H__
