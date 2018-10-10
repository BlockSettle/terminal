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
   WalletPasswordVerifyDialog(const std::string& walletId
      , const std::vector<bs::wallet::PasswordData>& keys
      , bs::wallet::KeyRank keyRank
      , const std::shared_ptr<ApplicationSettings> &appSettings
      , QWidget *parent = nullptr);
   ~WalletPasswordVerifyDialog();

private slots:
   void onContinueClicked();

private:
   void initPassword();
   void initFreja(const QString& frejaId);

   std::unique_ptr<Ui::WalletPasswordVerifyDialog> ui_;
   const std::string& walletId_;
   const std::vector<bs::wallet::PasswordData> keys_;
   const bs::wallet::KeyRank keyRank_;
   const std::shared_ptr<ApplicationSettings> appSettings_;
};

#endif // __WALLETPASSWORDVERIFYDIALOG_H__
