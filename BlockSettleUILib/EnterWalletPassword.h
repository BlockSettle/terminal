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
class ApplicationSettings;

class EnterWalletPassword : public QDialog
{
Q_OBJECT

public:
   EnterWalletPassword(const std::string &rootWalletId
      , const std::shared_ptr<ApplicationSettings> &appSettings
      , bs::wallet::KeyRank
      , const std::vector<bs::wallet::EncryptionType> &
      , const std::vector<SecureBinaryData> &encKeys = {}
      , const QString &prompt = {}
      , const QString &title = {}
      , QWidget* parent = nullptr);
   ~EnterWalletPassword() override;

   SecureBinaryData GetPassword() const;

private slots:
   void updateState();

protected:
   void reject() override;

private:
   std::unique_ptr<Ui::EnterWalletPassword> ui_;
};

#endif // __ENTER_WALLET_PASSWORD_H__
