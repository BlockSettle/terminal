#ifndef __ENTER_WALLET_PASSWORD_H__
#define __ENTER_WALLET_PASSWORD_H__

#include <memory>
#include <string>
#include <QDialog>
#include <QTimer>
#include "EncryptionUtils.h"
#include "AutheIDClient.h"
#include "QWalletInfo.h"
#include "WalletKeyWidget.h"
#include "ui_EnterWalletPassword.h"

namespace Ui {
    class EnterWalletPassword;
}
class ApplicationSettings;

class EnterWalletPassword : public QDialog
{
Q_OBJECT

public:
   explicit EnterWalletPassword(AutheIDClient::RequestType requestType
                                   , QWidget* parent = nullptr);
   ~EnterWalletPassword() override;

   void init(const bs::hd::WalletInfo &walletInfo
      , const std::shared_ptr<ApplicationSettings> &appSettings
      , const std::shared_ptr<ConnectionManager> &connectionManager
      , WalletKeyWidget::UseType useType
      , const QString &prompt
      , const std::shared_ptr<spdlog::logger> &logger
      , const QString &title = QString());


   bs::wallet::PasswordData passwordData(int keyIndex) const { return ui_->widgetSubmitKeys->passwordData(keyIndex); }
   SecureBinaryData resultingKey() const;
   std::vector<bs::wallet::PasswordData> passwordData() const { return ui_->widgetSubmitKeys->passwordData(); }

private slots:
   void updateState();

protected:
   void reject() override;

private:
   std::unique_ptr<Ui::EnterWalletPassword> ui_;
   AutheIDClient::RequestType requestType_{};
   bool fixEidAuth_;
   std::shared_ptr<spdlog::logger> logger_;
   bs::hd::WalletInfo walletInfo_;
   std::shared_ptr<ApplicationSettings> appSettings_;
};

#endif // __ENTER_WALLET_PASSWORD_H__
