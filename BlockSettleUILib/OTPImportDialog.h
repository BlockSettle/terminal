#ifndef __OTP_IMPORT_DIALOG_H__
#define __OTP_IMPORT_DIALOG_H__

#include <memory>
#include <unordered_set>

#include <QDialog>

#include "FrejaREST.h"
#include "EncryptionUtils.h"


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
      , const std::string &defaultUserName, QWidget* parent = nullptr );
   ~OTPImportDialog() override;

private slots:
   void accept() override;
   void keyTextChanged();
   void onFrejaIdChanged(const QString &);
   void startFrejaSign();
   void onFrejaSucceeded(SecureBinaryData);
   void onFrejaFailed(const QString &text);
   void onFrejaStatusUpdated(const QString &status);
   void updateAcceptButton();

private:
   Ui::OTPImportDialog* ui_;
   std::shared_ptr<OTPManager>   otpManager_;
   std::shared_ptr<EasyCoDec>    easyCodec_;
   FrejaSignOTP                  frejaSign_;
   SecureBinaryData              otpPassword_;
   EasyEncValidator           *  validator_ = nullptr;
   std::string hexKey_;
   bool        keyIsValid_ = false;
};

#endif // __OTP_IMPORT_DIALOG_H__
