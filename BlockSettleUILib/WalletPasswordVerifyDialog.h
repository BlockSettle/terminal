#ifndef __WALLETPASSWORDVERIFYDIALOG_H__
#define __WALLETPASSWORDVERIFYDIALOG_H__

#include <memory>
#include <QDialog>
#include "WalletEncryption.h"

namespace Ui {
   class WalletPasswordVerifyDialog;
}
class ApplicationSettings;

class WalletPasswordVerifyDialog : public QDialog
{
   Q_OBJECT

public:
   explicit WalletPasswordVerifyDialog(const std::shared_ptr<ApplicationSettings> &appSettings
      , QWidget *parent = nullptr);
   ~WalletPasswordVerifyDialog() override;

   // By default the dialog will show only Auth usage info page.
   // If init called then password/Auth check will be used too.
   void init(const std::string& walletId, const std::vector<bs::wallet::PasswordData>& keys
      , bs::wallet::KeyRank keyRank);

private slots:
   void onContinueClicked();

private:
   void initPassword();
   void initAuth(const QString& authId);

   std::unique_ptr<Ui::WalletPasswordVerifyDialog> ui_;
   std::string walletId_;
   std::vector<bs::wallet::PasswordData> keys_;
   bs::wallet::KeyRank keyRank_;
   bool runPasswordCheck_ = false;
   const std::shared_ptr<ApplicationSettings> appSettings_;
};

#endif // __WALLETPASSWORDVERIFYDIALOG_H__
