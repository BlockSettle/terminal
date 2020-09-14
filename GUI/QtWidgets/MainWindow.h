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
#include "SignerDefs.h"

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
class NotificationCenter;
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
            void onGetGeometry(const QRect &);
            void showStartupDialog(bool showLic);

            void onArmoryStateChanged(int state, unsigned int blockNum);
            void onNewBlock(int state, unsigned int blockNum);
            void onSignerStateChanged(int state, const std::string &);
            void onWalletsReady();

            void onHDWallet(const bs::sync::WalletInfo &);
            void onHDWalletDetails(const bs::sync::HDWalletData &);
            void onAddresses(const std::vector<bs::sync::Address> &);
            void onAddressComments(const std::string &walletId
               , const std::map<bs::Address, std::string> &);
            void onWalletBalance(const bs::sync::WalletBalanceData &);
            void onLedgerEntries(const std::string &filter, uint32_t totalPages
               , uint32_t curPage, uint32_t curBlock, const std::vector<bs::TXEntry> &);
            void onTXDetails(const std::vector<bs::sync::TXWalletDetails> &);

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
            void putSetting(int, const QVariant &);
            void createNewWallet();
            void needHDWalletDetails(const std::string &walletId);
            void needWalletBalances(const std::string &walletId);
            void needSpendableUTXOs(const std::string &walletId);

            void needExtAddresses(const std::string &walletId);
            void needIntAddresses(const std::string &walletId);
            void needUsedAddresses(const std::string &walletId);
            void needAddrComments(const std::string &walletId, const std::vector<bs::Address> &);
            void setAddrComment(const std::string &walletId, const bs::Address &
               , const std::string &comment);

            void needLedgerEntries(const std::string &filter);
            void needTXDetails(const std::vector<bs::sync::TXWallet> &);

         private slots:
            void onSend();
            void onGenerateAddress();

         //   void openAuthManagerDialog();
            void openConfigDialog(bool showInNetworkPage = false);
         //   void openAccountInfoDialog();
         //   void openCCTokenDialog();

            void onLoggedIn();
         //   void onLoginProceed(const NetworkSettings &networkSettings);
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
            void onUserLoggedIn();
            void onUserLoggedOut();

//            void onAccountTypeChanged(bs::network::UserType userType, bool enabled);

            void setLoginButtonText(const QString& text);

            void setupShortcuts();
            void setupInfoWidget();
            void setupIcon();
            void setupToolbar();
            void setupMenu();
            void setupTopRightWidget();

            void updateAppearance();
            void updateControlEnabledState();
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

         private:
            std::unique_ptr<Ui::BSTerminalMainWindow> ui_;
            std::shared_ptr<spdlog::logger>  logger_;
            std::shared_ptr<bs::message::QueueInterface> queue_;
            std::shared_ptr<bs::message::User>  guiUser_, settingsUser_;

            QAction *action_send_{ nullptr };
            QAction *action_generate_address_{ nullptr };
            QAction *action_login_{ nullptr };
            QAction *action_logout_{ nullptr };

            std::shared_ptr<StatusBarView>            statusBarView_;
            std::shared_ptr<QSystemTrayIcon>          sysTrayIcon_;
            std::shared_ptr<NotificationCenter>       notifCenter_;
            //   std::shared_ptr<TransactionsViewModel>    transactionsModel_;
            //   std::shared_ptr<CCPortfolioModel>         portfolioModel_;
            //   std::shared_ptr<OrderListModel>           orderListModel_;
            std::shared_ptr<AuthAddressDialog>        authAddrDlg_;

            std::shared_ptr<TransactionsViewModel>    txModel_;

            //   std::shared_ptr<WalletManagementWizard> walletsWizard_;

            QString currentUserLogin_;
            QString  loginButtonText_;
            QTimer * loginTimer_{};

            bool initialWalletCreateDialogShown_ = false;
            bool deferCCsync_ = false;

            std::queue<std::function<void(void)>> deferredDialogs_;
            bool deferredDialogRunning_ = false;
            //   bs::network::UserType userType_{};
         };
      }
   }
} // namespaces

#endif // __GUI_QT_MAIN_WINDOW_H__
