#ifndef __ENTER_OTP_PASSWORD_DIALOG_H__
#define __ENTER_OTP_PASSWORD_DIALOG_H__

#include <QDialog>

#include "EncryptionUtils.h"

namespace Ui {
    class EnterOTPPasswordDialog;
};
class OTPManager;

class EnterOTPPasswordDialog : public QDialog
{
Q_OBJECT

public:
   EnterOTPPasswordDialog(const std::shared_ptr<OTPManager> &, const QString& reason, QWidget* parent = nullptr);
   ~EnterOTPPasswordDialog() override;

   SecureBinaryData GetPassword() const { return password_; }

protected:
   void reject() override;

private slots:
   void passwordChanged();
   void onAuthSucceeded(SecureBinaryData);
   void onAuthFailed(const QString &);
   void onAuthStatusUpdated(const QString &);
   void authTimer();

private:
   std::unique_ptr<Ui::EnterOTPPasswordDialog> ui_;
   SecureBinaryData  password_;
   QTimer *authTimer_;
};

#endif // __ENTER_OTP_PASSWORD_DIALOG_H__
