#ifndef __SET_OTP_PASSWORD_DIALOG_H__
#define __SET_OTP_PASSWORD_DIALOG_H__

#include <QDialog>

namespace Ui {
    class SetOTPPasswordDialog;
};

class SetOTPPasswordDialog : public QDialog
{
Q_OBJECT

public:
   SetOTPPasswordDialog(QWidget* parent = nullptr );
   ~SetOTPPasswordDialog() override;

   QString GetPassword();

public slots:
   void passwordChanged();

private:
   void setStatusString(const QString& status);
   void clearStatusString();
private:
   std::unique_ptr<Ui::SetOTPPasswordDialog> ui_;
};

#endif // __SET_OTP_PASSWORD_DIALOG_H__
