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

   NewWalletPasswordVerifyDialog(const std::vector<bs::wallet::PasswordData>& keys, QWidget *parent = nullptr);
   ~NewWalletPasswordVerifyDialog();

private slots:
   void onContinueClicked();

private:
   void initPassword();
   void initFreja(const QString& frejaId);

   std::unique_ptr<Ui::NewWalletPasswordVerifyDialog> ui_;
   bs::wallet::EncryptionType encryptionType_{bs::wallet::EncryptionType::Unencrypted};
   SecureBinaryData password_;
};

#endif // __NEWWALLETPASSWORDVERIFYDIALOG_H__
