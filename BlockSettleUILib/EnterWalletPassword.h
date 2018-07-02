#ifndef __ENTER_WALLET_PASSWORD_H__
#define __ENTER_WALLET_PASSWORD_H__

#include <string>
#include <QDialog>
#include <QTimer>
#include "EncryptionUtils.h"
#include "MetaData.h"
#include "FrejaREST.h"


namespace Ui {
    class EnterWalletPassword;
};

class EnterWalletPassword : public QDialog
{
Q_OBJECT

public:
   EnterWalletPassword(const QString& walletName, const std::string &rootWalletId
      , bs::wallet::EncryptionType, const SecureBinaryData &encKey = {}
      , const QString &prompt = {}, QWidget* parent = nullptr);
   ~EnterWalletPassword() override = default;

   SecureBinaryData GetPassword() const { return password_; }

private slots:
   void PasswordChanged();

private:
   Ui::EnterWalletPassword* ui_;
   QTimer      timer_;
   float       timeLeft_ = 120;
   FrejaSignWallet   frejaSign_;
   SecureBinaryData  password_;
};

#endif // __ENTER_WALLET_PASSWORD_H__
