#ifndef __CHANGE_WALLET_PASSWORD_DIALOG_H__
#define __CHANGE_WALLET_PASSWORD_DIALOG_H__

#include <QDialog>

namespace Ui {
    class ChangeWalletPasswordDialog;
};

class ChangeWalletPasswordDialog : public QDialog
{
Q_OBJECT

public:
   ChangeWalletPasswordDialog(const QString& walletName, bool oldPasswordRequired, QWidget* parent = nullptr );
   ~ChangeWalletPasswordDialog() override = default;

   QString GetOldPassword() const;
   QString GetNewPassword() const;

private slots:
   void PasswordTextChanged();

private:
   Ui::ChangeWalletPasswordDialog* ui_;
   const bool oldPasswordRequired_;
};

#endif // __CHANGE_WALLET_PASSWORD_DIALOG_H__
