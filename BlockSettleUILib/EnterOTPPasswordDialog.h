#ifndef __ENTER_OTP_PASSWORD_DIALOG_H__
#define __ENTER_OTP_PASSWORD_DIALOG_H__

#include <QDialog>

namespace Ui {
    class EnterOTPPasswordDialog;
};

class EnterOTPPasswordDialog : public QDialog
{
Q_OBJECT

public:
   EnterOTPPasswordDialog(const QString& reason, QWidget* parent = nullptr );
   ~EnterOTPPasswordDialog() override = default;

   QString GetPassword();

private slots:
   void passwordChanged();

private:
   Ui::EnterOTPPasswordDialog* ui_;
};

#endif // __ENTER_OTP_PASSWORD_DIALOG_H__
