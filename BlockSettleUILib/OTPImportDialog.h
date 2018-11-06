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
class MobileClient;
class ApplicationSettings;

namespace spdlog {
class logger;
}

class OTPImportDialog : public QDialog
{
Q_OBJECT

public:
   OTPImportDialog(const std::shared_ptr<spdlog::logger> &logger
      , const std::shared_ptr<OTPManager>& otpManager
      , const std::string &defaultUserName
      , const std::shared_ptr<ApplicationSettings> &appSettings
      , QWidget* parent = nullptr );
   ~OTPImportDialog() override;

private slots:
   void accept() override;
   void keyTextChanged();
   void onAuthIdChanged(const QString &);
   void startAuthSign();
   void onAuthSucceeded(const std::string& encKey, const SecureBinaryData &password);
   void onAuthFailed(const QString &text);
   void onAuthStatusUpdated(const QString &status);
   void updateAcceptButton();

private:
   std::unique_ptr<Ui::OTPImportDialog> ui_;
   std::shared_ptr<spdlog::logger> logger_;
   std::shared_ptr<OTPManager>   otpManager_;
   std::shared_ptr<EasyCoDec>    easyCodec_;
   SecureBinaryData              otpPassword_;
   std::unique_ptr<EasyEncValidator>   validator_;
   SecureBinaryData otpKey_;
   bool        keyIsValid_ = false;
   MobileClient *mobileClient_ = nullptr;
};

#endif // __OTP_IMPORT_DIALOG_H__
