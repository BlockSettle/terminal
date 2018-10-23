#ifndef __OTP_IMPORT_DIALOG_H__
#define __OTP_IMPORT_DIALOG_H__

#include <memory>
#include <unordered_set>

#include <QDialog>

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
   void onAuthIdChanged(const QString &);
   void startAuthSign();
   void onAuthSucceeded(SecureBinaryData);
   void onAuthFailed(const QString &text);
   void onAuthStatusUpdated(const QString &status);
   void updateAcceptButton();

private:
   std::unique_ptr<Ui::OTPImportDialog> ui_;
   std::shared_ptr<OTPManager>   otpManager_;
   std::shared_ptr<EasyCoDec>    easyCodec_;
   SecureBinaryData              otpPassword_;
   std::unique_ptr<EasyEncValidator>   validator_;
   std::string hexKey_;
   bool        keyIsValid_ = false;
};

#endif // __OTP_IMPORT_DIALOG_H__
