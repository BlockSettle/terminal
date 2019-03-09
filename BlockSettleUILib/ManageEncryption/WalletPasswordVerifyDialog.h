#ifndef __WALLET_PASSWORD_VERIFY_DIALOG_H__
#define __WALLET_PASSWORD_VERIFY_DIALOG_H__

#include <memory>
#include <QDialog>
#include "WalletEncryption.h"
#include "QWalletInfo.h"

namespace Ui {
   class WalletPasswordVerifyDialog;
}
class ApplicationSettings;
class ConnectionManager;

class WalletPasswordVerifyDialog : public QDialog
{
   Q_OBJECT

public:
   explicit WalletPasswordVerifyDialog(const std::shared_ptr<ApplicationSettings> &appSettings
      , const std::shared_ptr<ConnectionManager> &connectionManager
      , QWidget *parent = nullptr);
   ~WalletPasswordVerifyDialog() override;

   // By default the dialog will show only Auth usage info page.
   // If init called then password/Auth check will be used too.
   void init(const bs::hd::WalletInfo &walletInfo
             , const std::vector<bs::wallet::PasswordData> &keys
             , const std::shared_ptr<spdlog::logger> &logger);

private slots:
   void onContinueClicked();

private:
   void initPassword();
   void initAuth(const QString& authId);

   std::unique_ptr<Ui::WalletPasswordVerifyDialog> ui_;
   std::vector<bs::wallet::PasswordData> keys_;
   bs::hd::WalletInfo walletInfo_;
   bool runPasswordCheck_ = false;
   std::shared_ptr<ApplicationSettings> appSettings_;
   std::shared_ptr<ConnectionManager> connectionManager_;
   std::shared_ptr<spdlog::logger> logger_;
};

#endif // __WALLET_PASSWORD_VERIFY_DIALOG_H__
