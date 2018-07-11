#include "OTPManager.h"

#include "OTPFile.h"
#include "CelerClient.h"
#include "ApplicationSettings.h"
#include "EncryptionUtils.h"

#include <cassert>
#include <spdlog/spdlog.h>

OTPManager::OTPManager(const std::shared_ptr<spdlog::logger>& logger
      , const std::shared_ptr<ApplicationSettings>& appSettings
      , const std::shared_ptr<CelerClient>& celerClient)
 : logger_(logger)
 , applicationSettings_(appSettings)
 , celerClient_(celerClient)
{
   otpFile_ = OTPFile::LoadFromFile(logger_, applicationSettings_->get<QString>(ApplicationSettings::otpFileName));

   connect(celerClient_.get(), &CelerClient::OnConnectedToServer, this, &OTPManager::onConnectedToCeler);
}

void OTPManager::onConnectedToCeler()
{
   SyncWithCelerAccount({});
}

std::shared_ptr<OTPFile> OTPManager::GetOTPForCurrentUser() const
{
   return otpFile_;
}

bool OTPManager::RemoveOTPForCurrentUser()
{
   if (otpFile_ != nullptr) {
      if (!otpFile_->RemoveFile()) {
         logger_->error("[OTPManager::RemoveOTPForCurrentUser] failed to remove OTP file");
         return false;
      }
      otpFile_.reset();
   }

   return true;
}

OTPManager::OTPImportResult OTPManager::ImportOTPForCurrentUser(const SecureBinaryData& rootKey
   , const SecureBinaryData &passphrase, bs::wallet::EncryptionType encType, const SecureBinaryData &encKey)
{
   std::shared_ptr<OTPFile> newFile = OTPFile::CreateFromPrivateKey(logger_
      , applicationSettings_->get<QString>(ApplicationSettings::otpFileName)
      , rootKey, encType, passphrase, encKey);

   if (newFile == nullptr) {
      logger_->error("[OTPManager::ImportOTPForCurrentUser] failed to import OTP from key");
      return OTPImportResult::InvalidKey;
   }

   if (celerClient_ && celerClient_->IsConnected()) {
      if (!celerClient_->getUserOtpId().empty()
       && (celerClient_->getUserOtpId() != newFile->GetOtpId())) {
         logger_->error("[OTPManager::ImportOTPForCurrentUser] attempt to import outdated OTP {}"
            , newFile->GetOtpId());
         return OTPImportResult::OutdatedOTP;
      }
   }

   if (!newFile->SyncToFile()) {
      logger_->error("[OTPManager::ImportOTPForCurrentUser] failed to save OTP to file");
      return OTPImportResult::FileError;
   }

   otpFile_ = newFile;

   SyncWithCelerAccount(passphrase);
   emit OTPImported();
   return OTPImportResult::Success;
}

bool OTPManager::CurrentUserHaveOTP() const
{
   return GetOTPForCurrentUser() != nullptr;
}

bool OTPManager::IsCurrentOTPLatest() const
{
   return otpLatest_;
}

bool OTPManager::CountAdvancingRequired() const
{
   return !otpCountOk_;
}

bool OTPManager::CanSign() const
{
   return CurrentUserHaveOTP() && IsCurrentOTPLatest() && !CountAdvancingRequired();
}

QString OTPManager::GetShortId() const
{
   auto otp = GetOTPForCurrentUser();
   assert(otp != nullptr);
   if (otp) {
      return otp->GetShortId();
   }
   return QString();
}

bs::wallet::EncryptionType OTPManager::GetEncType() const
{
   auto otp = GetOTPForCurrentUser();
   assert(otp != nullptr);
   if (otp) {
      return otp->encryptionType();
   }
   return bs::wallet::EncryptionType::Unencrypted;
}

QString OTPManager::GetEncKey() const
{
   auto otp = GetOTPForCurrentUser();
   assert(otp != nullptr);
   if (otp) {
      return QString::fromStdString(otp->encryptionKey().toBinStr());
   }
   return QString();
}

QString OTPManager::GetImportDateString() const
{
   auto otp = GetOTPForCurrentUser();
   assert(otp != nullptr);
   if (otp) {
      return otp->GetImportDateString();
   }

   return QString();
}

unsigned int OTPManager::GetUsedKeysCount() const
{
   auto otp = GetOTPForCurrentUser();
   assert(otp != nullptr);
   if (otp) {
      return otp->GetUsedKeysCount();
   }
   return 0;
}

bool OTPManager::Sign(const SecureBinaryData &data, const cbPassword &cbPass, const SignedCb &cbSigned)
{
   auto otp = GetOTPForCurrentUser();
   assert(otp && cbPass && cbSigned);
   SecureBinaryData password;
   password = cbPass(otp->encryptionType(), otp->encryptionKey());
   return Sign(data, password, cbSigned);
}

bool OTPManager::Sign(const SecureBinaryData &dataToSign, const SecureBinaryData& otpPassword, const SignedCb &onSigned)
{
   auto otp = GetOTPForCurrentUser();

   const auto result = otp->Sign(dataToSign, otpPassword);
   if (result.first.isNull()) {
      return false;
   }
   onSigned(result.first, otp->GetOtpId(), result.second);
   SyncWithCelerAccount(otpPassword);
   return true;
}

bool OTPManager::UpdatePassword(cbChangePassword cb)
{
   SecureBinaryData oldPass, newPass, encKey;
   bs::wallet::EncryptionType encType;
   const auto &otp = GetOTPForCurrentUser();
   if (otp && cb && cb(otp, oldPass, newPass, encType, encKey)) {
      logger_->debug("[OTPManager::UpdatePassword]");
      return otp->UpdateCurrentPrivateKey(oldPass, newPass, encType, encKey);
   }
   return false;
}

bool OTPManager::IsEncrypted() const
{
   const auto otp = GetOTPForCurrentUser();
   if (otp) {
      return (otp->encryptionType() != bs::wallet::EncryptionType::Unencrypted);
   }
   return false;
}

bool OTPManager::IsPasswordCorrect(const SecureBinaryData &password) const
{
   const auto otp = GetOTPForCurrentUser();
   if (otp) {
      return !otp->GetCurrentPrivateKey(password).isNull();
   }
   return false;
}

bool OTPManager::AdvanceOTPKey(const SecureBinaryData &password)
{
   if (otpCountOk_) {
      logger_->debug("[OTPManager::AdvanceOTPKey] OTP count is fine");
      return true;
   }

   if (!celerClient_ || !celerClient_->IsConnected()) {
      logger_->error("[OTPManager::AdvanceOTPKey] could not adnvace when disconnected from celer");
      return false;
   }

   auto otp = GetOTPForCurrentUser();
   if (otp == nullptr) {
      logger_->error("[OTPManager::AdvanceOTPKey] there is no OTP imported for user");
      return false;
   }

   logger_->debug("[OTPManager::AdvanceOTPKey] advancing from {} to  {}"
      , otp->GetUsedKeysCount(), celerClient_->getUserOtpUsedCount());

   if (!otp->AdvanceKeysTo(celerClient_->getUserOtpUsedCount(), password).isNull()) {
      otpCountOk_ = true;
      logger_->debug("[OTPManager::AdvanceOTPKey] OTP advanced to {}"
         , otp->GetUsedKeysCount());

      emit SyncCompleted();

      return true;
   }

   logger_->error("[OTPManager::AdvanceOTPKey] failed to advance");
   return false;
}

void OTPManager::SyncWithCelerAccount(const SecureBinaryData &password)
{
   otpLatest_ = false;
   otpCountOk_ = false;

   if (!celerClient_) {
      logger_->error("[OTPManager::SyncOTP] no client assigned");
      return;
   }

   if (!celerClient_->IsConnected()) {
      logger_->debug("[OTPManager::SyncOTP] not logged in. Sync is not available");
      return;
   }

   auto otp = GetOTPForCurrentUser();
   if (otp == nullptr) {
      logger_->debug("[OTPManager::SyncOTP] there is no OTP imported for user");
      emit SyncCompleted();
      return;
   }

   if (celerClient_->getUserOtpId() != otp->GetOtpId()) {
      logger_->error("[OTPManager::SyncOTP] customer {} have imported outdated OTP: current {}, registered {}"
         , celerClient_->userName(), otp->GetOtpId(), celerClient_->getUserOtpId());
   } else {
      otpLatest_ = true;
      if (celerClient_->getUserOtpUsedCount() != otp->GetUsedKeysCount()) {
         if (celerClient_->getUserOtpUsedCount() < otp->GetUsedKeysCount()) {
            logger_->debug("[OTPManager::SyncOTP] updating OTP usage count for {} to {}."
               , celerClient_->userName(), otp->GetOtpId());
            celerClient_->setUserOtpUsedCount(otp->GetUsedKeysCount());
            otpCountOk_ = true;
         } else {
            if (password.isNull()) {
               logger_->warn("[OTPManager::SyncOTP] OTP keys advancing requires OTP password now");
            }
            otpCountOk_ = !otp->AdvanceKeysTo(celerClient_->getUserOtpUsedCount(), password).isNull();
         }
      } else {
         otpCountOk_ = true;
      }
   }

   emit SyncCompleted();
}
