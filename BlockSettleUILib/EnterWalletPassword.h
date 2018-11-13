#ifndef __ENTER_WALLET_PASSWORD_H__
#define __ENTER_WALLET_PASSWORD_H__

#include <memory>
#include <string>
#include <QDialog>
#include <QTimer>
#include "EncryptionUtils.h"
#include "MetaData.h"
#include "MobileClientRequestType.h"

namespace Ui {
    class EnterWalletPassword;
};
class ApplicationSettings;

class EnterWalletPassword : public QDialog
{
Q_OBJECT

public:
   explicit EnterWalletPassword(MobileClientRequest requestType, QWidget* parent = nullptr);
   ~EnterWalletPassword() override;

   void init(const std::string &walletId, bs::wallet::KeyRank keyRank
      , const std::vector<bs::wallet::EncryptionType> &encTypes
      , const std::vector<SecureBinaryData> &encKeys
      , const std::shared_ptr<ApplicationSettings> &appSettings
      , const QString &prompt, const QString &title = QString());

   void init(const std::string &walletId, bs::wallet::KeyRank keyRank
      , const std::vector<bs::wallet::PasswordData> &keys
      , const std::shared_ptr<ApplicationSettings> &appSettings
      , const QString &prompt, const QString &title = QString());

   std::string getEncKey(int index) const;
   SecureBinaryData getPassword() const;

private slots:
   void updateState();

protected:
   void reject() override;

private:
   std::unique_ptr<Ui::EnterWalletPassword> ui_;
   MobileClientRequest requestType_{};
};

#endif // __ENTER_WALLET_PASSWORD_H__
