#ifndef __OTP_FILE_H__
#define __OTP_FILE_H__

#include <memory>

#include <QString>
#include <QDateTime>

#include "EncryptionUtils.h"
#include "MetaData.h"

namespace spdlog
{
   class logger;
}

class OTPFile
{
public:
   OTPFile(const std::shared_ptr<spdlog::logger>& logger
           , const QString& filePath
           , const std::string& otpId
           , const SecureBinaryData& chainCodePlain
           , const SecureBinaryData& nextPrivateKeyPlain
           , bs::wallet::EncryptionType encType = bs::wallet::EncryptionType::Unencrypted
           , const SecureBinaryData &encKey = {}
           , const BinaryData &hash = {}
           , unsigned int usedKeysCount = 0
           , const QString& importDate = QDateTime::currentDateTime().toString());

   static std::shared_ptr<OTPFile> LoadFromFile(const std::shared_ptr<spdlog::logger>& logger
      , const QString& filePath);

   static std::shared_ptr<OTPFile> CreateFromPrivateKey(const std::shared_ptr<spdlog::logger>& logger
      , const QString& filePath, const SecureBinaryData& privateKey, bs::wallet::EncryptionType
      , const SecureBinaryData &password = {}, const SecureBinaryData &encKey = {});

   OTPFile() = delete;
   ~OTPFile() noexcept = default;

   OTPFile(const OTPFile&) = delete;
   OTPFile& operator = (const OTPFile&) = delete;
   OTPFile(OTPFile&&) = delete;
   OTPFile& operator = (OTPFile&&) = delete;

   QString     GetImportDateString() const { return importDate_; }
   std::string GetOtpId() const { return otpId_; }
   SecureBinaryData GetChainCode() const { return chainCode_; }
   QString     GetShortId() const { return QString::fromStdString(otpId_.substr(0, 8)); }
   bs::wallet::EncryptionType encryptionType() const { return encType_; }
   SecureBinaryData encryptionKey() const { return encKey_; }

   unsigned int GetUsedKeysCount() const { return usedKeysCount_; }
   [[deprecated]] SecureBinaryData  GetCurrentPrivateKey(const SecureBinaryData &password = {}) const;
   bool UpdateCurrentPrivateKey(const SecureBinaryData &oldPass, const SecureBinaryData &newPass
      , bs::wallet::EncryptionType encType = bs::wallet::EncryptionType::Password, const SecureBinaryData &encKey = {});
   SecureBinaryData AdvanceKeysTo(unsigned int usedKeys, const SecureBinaryData &password);

   std::pair<SecureBinaryData, unsigned int> Sign(const SecureBinaryData &data
      , const SecureBinaryData &password = {});
   bool CheckSignature(const SecureBinaryData &data, const SecureBinaryData &signature, unsigned int keyIndex
      , const SecureBinaryData &password = {});

   bool SyncToFile();
   bool RemoveFile();

private:
   bool makeBackup();
   bool removeBackup();
   bool restoreBackup();

   static SecureBinaryData GetNextKeyInChain(const SecureBinaryData& privateKey, const SecureBinaryData& chainCode);
   static SecureBinaryData Encrypt(SecureBinaryData data, SecureBinaryData key, BinaryData &hash);
   static SecureBinaryData Decrypt(SecureBinaryData data, SecureBinaryData key, const BinaryData &hash);
   static void PadKey(SecureBinaryData &key);

private:
   std::shared_ptr<spdlog::logger>  logger_;
   QString                    otpFilePath_;

   std::string                otpId_;
   QString                    importDate_;
   SecureBinaryData           chainCode_;
   unsigned int               usedKeysCount_;
   SecureBinaryData           nextUnusedPrivateKey_;
   bs::wallet::EncryptionType encType_ = bs::wallet::EncryptionType::Unencrypted;
   SecureBinaryData           encKey_;
   BinaryData                 hash_;
};

#endif // __OTP_FILE_H__
