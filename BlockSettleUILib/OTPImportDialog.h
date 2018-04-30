#ifndef __OTP_IMPORT_DIALOG_H__
#define __OTP_IMPORT_DIALOG_H__

#include <memory>
#include <unordered_set>
#include <QDialog>


namespace Ui {
    class OTPImportDialog;
}
class OTPManager;
class EasyCoDec;
class EasyEncValidator;


class OTPImportDialog : public QDialog
{
Q_OBJECT

public:
   OTPImportDialog(const std::shared_ptr<OTPManager>& otpManager
      , QWidget* parent = nullptr );
   ~OTPImportDialog() override;

private slots:
   void onPasswordChanged();
   void accept() override;
   void keyTextChanged();

private:
   Ui::OTPImportDialog* ui_;
   std::shared_ptr<OTPManager>   otpManager_;
   std::shared_ptr<EasyCoDec>    easyCodec_;
   EasyEncValidator           *  validator_ = nullptr;
   std::string hexKey_;
   bool        keyIsValid_ = false;
};

#endif // __OTP_IMPORT_DIALOG_H__
