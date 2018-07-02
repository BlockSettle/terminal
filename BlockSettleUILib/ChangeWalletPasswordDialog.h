#ifndef __CHANGE_WALLET_PASSWORD_DIALOG_H__
#define __CHANGE_WALLET_PASSWORD_DIALOG_H__

#include <memory>
#include <QDialog>
#include "EncryptionUtils.h"
#include "FrejaREST.h"
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
   ChangeWalletPasswordDialog(const std::shared_ptr<bs::hd::Wallet> &, bs::wallet::EncryptionType
      , const SecureBinaryData &encKey, QWidget* parent = nullptr );
   ~ChangeWalletPasswordDialog() override = default;

   SecureBinaryData GetOldPassword() const { return oldPassword_; }
   SecureBinaryData GetNewPassword() const { return newPassword_; }
   bs::wallet::EncryptionType GetNewEncryptionType() const;
   SecureBinaryData GetNewEncryptionKey() const;

private slots:
   void accept() override;
   void PasswordTextChanged();
   void FrejaIdChanged();
   void EncTypeSelectionClicked();

private:
   Ui::ChangeWalletPasswordDialog * ui_;
   std::shared_ptr<bs::hd::Wallet>  wallet_;
   const bs::wallet::EncryptionType encType_;
   const SecureBinaryData           encKey_;
   FrejaSignWallet                  frejaSignOld_;
   FrejaSignWallet                  frejaSignNew_;
   SecureBinaryData                 oldPassword_;
   SecureBinaryData                 newPassword_;
};

#endif // __CHANGE_WALLET_PASSWORD_DIALOG_H__
