#ifndef __OTP_FILE_MANAGER_H__
#define __OTP_FILE_MANAGER_H__

#include <functional>
#include <memory>

#include <QObject>
#include <QString>
#include "MetaData.h"


namespace spdlog {
   class logger;
}
class CelerClient;
class OTPFile;
class SecureBinaryData;
class ApplicationSettings;

class OTPManager : public QObject
{
Q_OBJECT
public:
   enum class OTPImportResult
   {
      Success,
      InvalidKey,
      FileError,
      OutdatedOTP
   };

public:
   using cbPassword = std::function<SecureBinaryData (void)>;
   using cbChangePassword = std::function<bool (const std::shared_ptr<OTPFile> &, SecureBinaryData &oldPass
      , SecureBinaryData &newPass, bs::wallet::EncryptionType &, SecureBinaryData &encKey)>;

   OTPManager(const std::shared_ptr<spdlog::logger>& logger
      , const std::shared_ptr<ApplicationSettings>& appSettings
      , const std::shared_ptr<CelerClient>& celerClient);
   OTPManager(const std::shared_ptr<spdlog::logger>& logger, const std::shared_ptr<OTPFile>& otpFile)
      : logger_(logger), otpFile_(otpFile) {}
   ~OTPManager() noexcept = default;

   OTPManager(const OTPManager&) = delete;
   OTPManager& operator = (const OTPManager&) = delete;

   OTPManager(OTPManager&&) = delete;
   OTPManager& operator = (OTPManager&&) = delete;

   bool              CurrentUserHaveOTP() const;
   bool              IsCurrentOTPLatest() const;
   bool              CountAdvancingRequired() const;

   bool              AdvanceOTPKey(const SecureBinaryData &password);

   bool              CanSign() const;

   QString           GetShortId() const;
   QString           GetImportDateString() const;

   bs::wallet::EncryptionType GetEncType() const;
   QString           GetEncKey() const;

   unsigned int      GetUsedKeysCount() const;
   bool UpdatePassword(cbChangePassword);
   bool IsEncrypted() const;
   bool IsPasswordCorrect(const SecureBinaryData &password) const;

   OTPImportResult   ImportOTPForCurrentUser(const SecureBinaryData& rootKey, const SecureBinaryData &passphrase
      , bs::wallet::EncryptionType encType = bs::wallet::EncryptionType::Password, const SecureBinaryData &encKey = {});
   bool              RemoveOTPForCurrentUser();

   using SignedCb = std::function<void (const SecureBinaryData &signature, const std::string &otpId
      , unsigned int keyIndex)>;
   bool Sign(const SecureBinaryData &, const cbPassword &, const SignedCb &);
   bool Sign(const SecureBinaryData &dataToSign, const SecureBinaryData& otpPassword, const SignedCb &);

private:
   std::shared_ptr<OTPFile> GetOTPForCurrentUser() const;
   void SyncWithCelerAccount(const SecureBinaryData &password);

signals:
   void OTPImported();

   void SyncCompleted();

private slots:
   void onConnectedToCeler();

private:
   std::shared_ptr<spdlog::logger>        logger_;
   std::shared_ptr<OTPFile>               otpFile_;

   bool otpLatest_ = false;
   bool otpCountOk_ = false;

   std::shared_ptr<ApplicationSettings>   applicationSettings_;
   std::shared_ptr<CelerClient>           celerClient_;
};

#endif // __OTP_FILE_MANAGER_H__