#ifndef __OTP_FILE_INFO_DIALOG_H__
#define __OTP_FILE_INFO_DIALOG_H__

#include <QDialog>
#include <memory>

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

private slots:
   void RemoveOTP();
   void onChangePwdClicked();
   void onPasswordChanged();
   void accept() override;

private:
   bool UpdateOTPCounter();

private:
   Ui::OTPFileInfoDialog* ui_;
   std::shared_ptr<OTPManager> otpManager_;
};

#endif // __OTP_FILE_INFO_DIALOG_H__
