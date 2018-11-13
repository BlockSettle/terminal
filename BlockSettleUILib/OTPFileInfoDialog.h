#ifndef __OTP_FILE_INFO_DIALOG_H__
#define __OTP_FILE_INFO_DIALOG_H__

#include <QDialog>
#include <memory>

#include "EncryptionUtils.h"

namespace Ui {
    class OTPFileInfoDialog;
}

namespace spdlog {
   class logger;
}

class ApplicationSettings;
class OTPManager;

class OTPFileInfoDialog : public QDialog
{
Q_OBJECT

public:
   OTPFileInfoDialog(const std::shared_ptr<spdlog::logger> &logger
      , const std::shared_ptr<OTPManager>& otpManager
      , const std::shared_ptr<ApplicationSettings> &settings
      , QWidget* parent = nullptr );
   ~OTPFileInfoDialog() override;

protected:
   void accept() override;
   void reject() override;

private slots:
   void RemoveOTP();
   void onChangePwdClicked();
   void onPasswordChanged();
   void onEncTypeClicked();
   void onAuthIdChanged();
   void onAuthClicked();
   void onAuthOldSucceeded(SecureBinaryData);
   void onAuthOldFailed(const QString &text);
   void onAuthOldStatusUpdated(const QString &status);
   void onAuthNewSucceeded(SecureBinaryData);
   void onAuthNewFailed(const QString &text);
   void onAuthNewStatusUpdated(const QString &status);

private:
   bool UpdateOTPCounter();

private:
   std::unique_ptr<Ui::OTPFileInfoDialog> ui_;
   std::shared_ptr<spdlog::logger> logger_;
   std::shared_ptr<OTPManager> otpManager_;
   SecureBinaryData  oldPassword_, newPassword_;
   std::shared_ptr<ApplicationSettings> settings_;
};

#endif // __OTP_FILE_INFO_DIALOG_H__
