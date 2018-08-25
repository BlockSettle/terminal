#ifndef __NEWWALLETPASSWORDVERIFYDIALOG_H__
#define __NEWWALLETPASSWORDVERIFYDIALOG_H__

#include <memory>
#include <QDialog>
#include "WalletEncryption.h"

namespace Ui {
   class NewWalletPasswordVerifyDialog;
}

class NewWalletPasswordVerifyDialog : public QDialog
{
   Q_OBJECT

public:
   enum Pages {
      FrejaInfo,
      Check,
   };

   NewWalletPasswordVerifyDialog(const std::string& walletId
      , const std::vector<bs::wallet::PasswordData>& keys, bs::wallet::KeyRank keyRank
      , QWidget *parent = nullptr);
   ~NewWalletPasswordVerifyDialog();

private slots:
   void onContinueClicked();

private:
   void initPassword();
   void initFreja(const QString& frejaId);

   std::unique_ptr<Ui::NewWalletPasswordVerifyDialog> ui_;
   const std::string& walletId_;
   const std::vector<bs::wallet::PasswordData> keys_;
   const bs::wallet::KeyRank keyRank_;
};

#endif // __NEWWALLETPASSWORDVERIFYDIALOG_H__
