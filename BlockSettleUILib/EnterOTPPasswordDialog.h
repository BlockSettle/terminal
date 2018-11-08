#ifndef __ENTER_OTP_PASSWORD_DIALOG_H__
#define __ENTER_OTP_PASSWORD_DIALOG_H__

#include <QDialog>

#include "EncryptionUtils.h"
#include "MobileClient.h"

namespace Ui {
    class EnterOTPPasswordDialog;
}
class OTPManager;
class MobileClient;
class ApplicationSettings;

namespace spdlog {
class logger;
}

class EnterOTPPasswordDialog : public QDialog
{
Q_OBJECT

public:
   EnterOTPPasswordDialog(const std::shared_ptr<spdlog::logger> &logger
      , const std::shared_ptr<OTPManager> &
      , const QString& reason, const std::shared_ptr<ApplicationSettings> &appSettings
      , QWidget* parent = nullptr);
   ~EnterOTPPasswordDialog() override;

   SecureBinaryData GetPassword() const { return password_; }

protected:
   void reject() override;

private slots:
   void passwordChanged();
   void onAuthSucceeded(const std::string& encKey, const SecureBinaryData &password);
   void onAuthFailed(const QString &);
   void onAuthStatusUpdated(const QString &);
   void authTimer();

private:
   std::unique_ptr<Ui::EnterOTPPasswordDialog> ui_;
   std::shared_ptr<spdlog::logger> logger_;
   SecureBinaryData  password_;
   QTimer *authTimer_ = nullptr;
   MobileClient *mobileClient_ = nullptr;
};

#endif // __ENTER_OTP_PASSWORD_DIALOG_H__
