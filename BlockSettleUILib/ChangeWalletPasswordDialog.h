#ifndef __CHANGE_WALLET_PASSWORD_DIALOG_H__
#define __CHANGE_WALLET_PASSWORD_DIALOG_H__

#include <memory>
#include <QDialog>
#include "EncryptionUtils.h"
#include "HDNode.h"
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
      , const std::vector<SecureBinaryData> &encKeys, bs::hd::KeyRank, QWidget* parent = nullptr);
   ~ChangeWalletPasswordDialog() override = default;

   SecureBinaryData oldPassword() const;
   std::vector<bs::hd::PasswordData> newPasswordData() const;
   bs::hd::KeyRank newKeyRank() const;

private slots:
   void updateState();

protected:
   void reject() override;
   void showEvent(QShowEvent *) override;

private:
   Ui::ChangeWalletPasswordDialog * ui_;
   std::shared_ptr<bs::hd::Wallet>  wallet_;
};

#endif // __CHANGE_WALLET_PASSWORD_DIALOG_H__
