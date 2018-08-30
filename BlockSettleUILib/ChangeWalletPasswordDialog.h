#ifndef __CHANGE_WALLET_PASSWORD_DIALOG_H__
#define __CHANGE_WALLET_PASSWORD_DIALOG_H__

#include <memory>
#include <QDialog>
#include "EncryptionUtils.h"
#include "MetaData.h"

namespace Ui {
    class ChangeWalletPasswordDialog;
}
namespace bs {
   namespace hd {
      class Wallet;
   }
}

class ChangeWalletPasswordDialog : public QDialog
{
Q_OBJECT

public:
   ChangeWalletPasswordDialog(const std::shared_ptr<bs::hd::Wallet> &, const std::vector<bs::wallet::EncryptionType> &
      , const std::vector<SecureBinaryData> &encKeys, bs::wallet::KeyRank
      , const QString& username, QWidget* parent = nullptr);
   ~ChangeWalletPasswordDialog() override;

   SecureBinaryData oldPassword() const;
   std::vector<bs::wallet::PasswordData> newPasswordData() const;
   bs::wallet::KeyRank newKeyRank() const;

private slots:
   void onContinueClicked();

protected:
   void reject() override;

private:
   std::unique_ptr<Ui::ChangeWalletPasswordDialog> ui_;
   std::shared_ptr<bs::hd::Wallet>  wallet_;
   const bs::wallet::KeyRank oldKeyRank_;
   std::vector<bs::wallet::PasswordData> oldPasswordData_;
   std::vector<bs::wallet::PasswordData> newPasswordData_;
   SecureBinaryData oldKey_;
};

#endif // __CHANGE_WALLET_PASSWORD_DIALOG_H__
