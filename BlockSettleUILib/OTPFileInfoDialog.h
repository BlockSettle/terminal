#ifndef __OTP_FILE_INFO_DIALOG_H__
#define __OTP_FILE_INFO_DIALOG_H__

#include <QDialog>
#include <memory>
#include "FrejaREST.h"

namespace Ui {
    class OTPFileInfoDialog;
};

class OTPManager;

class OTPFileInfoDialog : public QDialog
{
Q_OBJECT

public:
   OTPFileInfoDialog(const std::shared_ptr<OTPManager>& otpManager
      , QWidget* parent = nullptr );
   ~OTPFileInfoDialog() override = default;

protected:
   void accept() override;
   void reject() override;

private slots:
   void RemoveOTP();
   void onChangePwdClicked();
   void onPasswordChanged();
   void onEncTypeClicked();
   void onFrejaIdChanged();
   void onFrejaClicked();
   void onFrejaOldSucceeded(SecureBinaryData);
   void onFrejaOldFailed(const QString &text);
   void onFrejaOldStatusUpdated(const QString &status);
   void onFrejaNewSucceeded(SecureBinaryData);
   void onFrejaNewFailed(const QString &text);
   void onFrejaNewStatusUpdated(const QString &status);

private:
   bool UpdateOTPCounter();

private:
   Ui::OTPFileInfoDialog* ui_;
   std::shared_ptr<OTPManager> otpManager_;
   FrejaSignOTP      frejaOld_, frejaNew_;
   SecureBinaryData  oldPassword_, newPassword_;
};

#endif // __OTP_FILE_INFO_DIALOG_H__
