#ifndef __ENTER_WALLET_PASSWORD_H__
#define __ENTER_WALLET_PASSWORD_H__

#include <string>
#include <QDialog>
#include <QTimer>
#include "EncryptionUtils.h"
#include "MetaData.h"


namespace Ui {
    class EnterWalletPassword;
};

class EnterWalletPassword : public QDialog
{
Q_OBJECT

public:
   explicit EnterWalletPassword(QWidget* parent = nullptr);
   ~EnterWalletPassword() override;

   void init(const std::string &walletId, bs::wallet::KeyRank keyRank
      , const std::vector<bs::wallet::EncryptionType> &encTypes
      , const std::vector<SecureBinaryData> &encKeys, const QString &prompt);
   void init(const std::string &walletId, bs::wallet::KeyRank keyRank
      , const std::vector<bs::wallet::PasswordData> &keys, const QString &prompt);

   SecureBinaryData GetPassword() const;

private slots:
   void updateState();

protected:
   void reject() override;

private:
   std::unique_ptr<Ui::EnterWalletPassword> ui_;
};

#endif // __ENTER_WALLET_PASSWORD_H__
