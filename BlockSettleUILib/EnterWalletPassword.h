#ifndef __ENTER_WALLET_PASSWORD_H__
#define __ENTER_WALLET_PASSWORD_H__

#include <string>
#include <QDialog>
#include <QTimer>
#include "EncryptionUtils.h"
#include "HDNode.h"
#include "MetaData.h"


namespace Ui {
    class EnterWalletPassword;
};

class EnterWalletPassword : public QDialog
{
Q_OBJECT

public:
   EnterWalletPassword(const QString& walletName, const std::string &rootWalletId
      , bs::hd::KeyRank, const std::vector<bs::wallet::EncryptionType> &
      , const std::vector<SecureBinaryData> &encKeys = {}, const QString &prompt = {}
      , QWidget* parent = nullptr);
   ~EnterWalletPassword() override = default;

   SecureBinaryData GetPassword() const;

private slots:
   void updateState();

protected:
   void reject() override;
   void showEvent(QShowEvent *) override;

private:
   Ui::EnterWalletPassword* ui_;
};

#endif // __ENTER_WALLET_PASSWORD_H__
