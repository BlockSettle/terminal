#include "OTPFile.h"

#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QDateTime>

#include <spdlog/spdlog.h>

#include "cryptlib.h"
#include "filters.h"
#include "hmac.h"
#include "sha.h"

#include "bs_storage.pb.h"


std::shared_ptr<OTPFile> OTPFile::LoadFromFile(
   const std::shared_ptr<spdlog::logger>& logger
   , const QString& filePath)
{
   QFileInfo fileInfo(filePath);
   if (!fileInfo.exists()) {
      logger->debug("[OTPFile::LoadFromFile] OTP file not found. You need to import one.");
      return nullptr;
   }

   QFile file(filePath);
   if (!file.open(QIODevice::ReadOnly)) {
      logger->error("[OTPFile::LoadFromFile] failed to open OTP file for read");
      return nullptr;
   }

   QByteArray data = file.readAll();
   Blocksettle::Storage::OTPFile protoFile;

   if (!protoFile.ParseFromArray(data.data(), data.size())) {
      logger->error("[OTPFile::LoadFromFile] failed to parse OTP file");
      return nullptr;
   }

   bs::wallet::EncryptionType encType = bs::wallet::EncryptionType::Unencrypted;
   if (protoFile.has_encrypted() && protoFile.encrypted()) {
      encType = bs::wallet::EncryptionType::Password;
   }
   if (protoFile.has_encryptiontype()) {
      encType = static_cast<bs::wallet::EncryptionType>(protoFile.encryptiontype());
   }

   return std::make_shared<OTPFile>(logger
                        , filePath
                        , protoFile.otpid()
                        , protoFile.chaincode()
                        , protoFile.nextprivatekey()
                        , encType, protoFile.encryptionkey()
                        , protoFile.hash()
                        , protoFile.usedkeyscount()
                        , QString::fromStdString(protoFile.importdatestring()));
}

std::shared_ptr<OTPFile> OTPFile::CreateFromPrivateKey(
   const std::shared_ptr<spdlog::logger>& logger
   , const QString& filePath
   , const SecureBinaryData& privateKey, bs::wallet::EncryptionType encType
   , const SecureBinaryData &password, const SecureBinaryData &encKey)
{
   if (!filePath.isEmpty()) {
      QFileInfo fileInfo(filePath);

      if (fileInfo.exists()) {
         logger->error("[OTPFile::CreateFromPrivateKey] OTP file already exists");
         return nullptr;
      }
   }

   std::string chainCodeString;
   try {
      std::string key = privateKey.toBinStr();
      CryptoPP::HMAC<CryptoPP::SHA256> hmacEncoder((byte*)key.c_str(), key.length());
      CryptoPP::StringSource("Chain code for OTP", true,
                             new CryptoPP::HashFilter(hmacEncoder,
                                                      new CryptoPP::StringSink(chainCodeString)));
   } catch(const CryptoPP::Exception& e) {
      logger->error("[OTPFile::CreateFromPrivateKey] failed to calculate HMAC256 {}", e.what());
      return nullptr;
   }

   SecureBinaryData chainCode(chainCodeString);

   CryptoECDSA crypto;
   SecureBinaryData otpIdKey = crypto.CompressPoint(crypto.ComputePublicKey(privateKey));
   if (otpIdKey.isNull()) {
      logger->error("[OTPFile::CreateFromPrivateKey] failed to calculate otp key");
      return nullptr;
   }

   SecureBinaryData firstKey = GetNextKeyInChain(privateKey, chainCode);
   if (firstKey.isNull()) {
      logger->error("[OTPFile::CreateFromPrivateKey] failed to calculate first key in chain");
      return nullptr;
   }

   BinaryData hash;
   if (encType != bs::wallet::EncryptionType::Unencrypted) {
      firstKey = Encrypt(firstKey, password, hash);
   }

   return std::make_shared<OTPFile>(logger, filePath, otpIdKey.toHexStr(), chainCode, firstKey
      , encType, encKey, hash);
}

SecureBinaryData OTPFile::GetNextKeyInChain(const SecureBinaryData& privateKey, const SecureBinaryData& chainCode)
{
   return CryptoECDSA().ComputeChainedPrivateKey(privateKey, chainCode);
}

void OTPFile::PadKey(SecureBinaryData &key)
{
   const auto keyRem = key.getSize() % BTC_AES::BLOCKSIZE;
   if (keyRem) {
      key.resize(key.getSize() - keyRem + BTC_AES::BLOCKSIZE);
   }
}

SecureBinaryData OTPFile::Encrypt(SecureBinaryData data, SecureBinaryData key, BinaryData &hash)
{
   SecureBinaryData iv(BTC_AES::BLOCKSIZE);
   auto logger = spdlog::get("");
   PadKey(key);
   try {
      hash = BtcUtils::hash256(data);
      return CryptoAES().EncryptCBC(data, key, iv);
   }
   catch (const std::exception &e) {
      logger->error("Encrypt exception: {}", e.what());
      return {};
   }
   catch (...) {
      logger->error("Encrypt exception");
      return {};
   }
}

SecureBinaryData OTPFile::Decrypt(SecureBinaryData data, SecureBinaryData key, const BinaryData &hash)
{
   const SecureBinaryData iv(BTC_AES::BLOCKSIZE);
   PadKey(key);
   try {
      const auto result = CryptoAES().DecryptCBC(data, key, iv);
      if (hash != BtcUtils::hash256(result)) {
         return {};
      }
      return result;
   }
   catch (...) {
      return {};
   }
}

OTPFile::OTPFile(const std::shared_ptr<spdlog::logger>& logger
        , const QString& filePath
        , const string &otpId, const SecureBinaryData& chainCodePlain
        , const SecureBinaryData& nextPrivateKeyPlain
        , bs::wallet::EncryptionType encType
        , const SecureBinaryData &encKey
        , const BinaryData &hash
        , unsigned int usedKeysCount
        , const QString& importDate)
   : logger_(logger)
   , otpFilePath_(filePath)
   , otpId_(otpId)
   , importDate_(importDate)
   , chainCode_(chainCodePlain)
   , usedKeysCount_(usedKeysCount)
   , nextUnusedPrivateKey_(nextPrivateKeyPlain)
   , encType_(encType)
   , encKey_(encKey)
   , hash_(hash)
{}

bool OTPFile::SyncToFile()
{
   if (otpFilePath_.isEmpty()) {
      return true;
   }

   QFileInfo fileInfo(otpFilePath_);
   if (!fileInfo.absoluteDir().exists()) {
      QString path = fileInfo.absoluteDir().absolutePath();
      if (!QDir().mkdir(path)) {
         logger_->error("[OTPFile::SyncToFile] failed to create dir for otp file");
         return false;
      }
   }

   if (!makeBackup()) {
      logger_->error("[OTPFile::SyncToFile] failed to create backup file. Aborting save");
      return false;
   }

   Blocksettle::Storage::OTPFile protoFile;

   protoFile.set_importdatestring(importDate_.toStdString());
   protoFile.set_otpid(otpId_);
   protoFile.set_usedkeyscount(usedKeysCount_);
   protoFile.set_nextprivatekey(nextUnusedPrivateKey_.toBinStr());
   protoFile.set_chaincode(chainCode_.toBinStr());
   protoFile.set_encryptiontype(static_cast<uint8_t>(encType_));
   if (encType_ != bs::wallet::EncryptionType::Unencrypted) {
      protoFile.set_hash(hash_.toBinStr());
      if (encType_ == bs::wallet::EncryptionType::Freja) {
         protoFile.set_encryptionkey(encKey_.toBinStr());
      }
   }

   std::string data = protoFile.SerializeAsString();

   QFile outputFile(otpFilePath_);

   if (!outputFile.open(QIODevice::WriteOnly | QIODevice::Unbuffered)) {
      logger_->error("[OTPFile::SyncToFile] failed to open file for write");
      if (!restoreBackup()) {
         logger_->error("[OTPFile::SyncToFile] failed to restore backup file.");
      } else {
         removeBackup();
      }
      return false;
   }

   if (data.size() != outputFile.write(data.c_str(), data.length())) {
      // XXX - OTP file might be corrupted. remove it
      logger_->error("[OTPFile::SyncToFile] failed to write data to file");
      if (!restoreBackup()) {
         logger_->error("[OTPFile::SyncToFile] failed to restore backup file.");
      } else {
         removeBackup();
      }
      return false;
   }

   if (!removeBackup()) {
      logger_->warn("[OTPFile::SyncToFile] failed to remove OTP backup file");
   }

   return true;
}

bool OTPFile::RemoveFile()
{
   QFile otpFile(otpFilePath_);
   return otpFile.remove();
}

bool OTPFile::makeBackup()
{
   return true;
}

bool OTPFile::removeBackup()
{
   return true;
}

bool OTPFile::restoreBackup()
{
   return true;
}

SecureBinaryData OTPFile::GetCurrentPrivateKey(const SecureBinaryData &password) const
{
   const bool encrypted = (encType_ != bs::wallet::EncryptionType::Unencrypted);
   if (encrypted && password.isNull()) {
      return {};
   }
   return encrypted ? Decrypt(nextUnusedPrivateKey_, password, hash_) : nextUnusedPrivateKey_;
}

SecureBinaryData OTPFile::AdvanceKeysTo(unsigned int usedKeys, const SecureBinaryData &password)
{
   const bool encrypted = (encType_ != bs::wallet::EncryptionType::Unencrypted);
   if ((usedKeys < usedKeysCount_) || (encrypted && password.isNull())) {
      logger_->warn("[OTPFile::AdvanceKeysTo] wrong input data");
      return {};
   }

   SecureBinaryData result = GetCurrentPrivateKey(password);
   if ((usedKeys == usedKeysCount_) || result.isNull()) {
      logger_->warn("[OTPFile::AdvanceKeysTo] failed to get or decrypt private key");
      return result;
   }

   nextUnusedPrivateKey_ = result;
   while (usedKeysCount_ < usedKeys) {
      usedKeysCount_++;
      nextUnusedPrivateKey_ = GetNextKeyInChain(nextUnusedPrivateKey_, chainCode_);
   }
   if (encrypted) {
      nextUnusedPrivateKey_ = Encrypt(nextUnusedPrivateKey_, password, hash_);
   }

   if (!SyncToFile()) {
      result.clear();
   }
   return result;
}

bool OTPFile::UpdateCurrentPrivateKey(const SecureBinaryData &oldPass, const SecureBinaryData &newPass
   , bs::wallet::EncryptionType encType, const SecureBinaryData &encKey)
{
   const auto decrypted = GetCurrentPrivateKey(oldPass);
   if (decrypted.isNull()) {
      logger_->error("[OTPFile::UpdateCurrentPrivateKey] failed to decrypt");
      return false;
   }
   if (encType == bs::wallet::EncryptionType::Unencrypted) {
      nextUnusedPrivateKey_ = decrypted;
   }
   else {
      logger_->debug("newPass size = {}", newPass.getSize());
      nextUnusedPrivateKey_ = Encrypt(decrypted, newPass, hash_);
      if (nextUnusedPrivateKey_.isNull()) {
         logger_->error("[OTPFile::UpdateCurrentPrivateKey] failed to encrypt");
         return false;
      }
      encKey_ = encKey;
   }
   encType_ = encType;
   return SyncToFile();
}

std::pair<SecureBinaryData, unsigned int> OTPFile::Sign(const SecureBinaryData &data, const SecureBinaryData &password)
{
   const auto keyIndex = usedKeysCount_;
   const auto privKey = AdvanceKeysTo(usedKeysCount_ + 1, password);
   if (privKey.isNull()) {
      return { {}, UINT_MAX };
   }
   return { CryptoECDSA().SignData(data, privKey), keyIndex };
}

bool OTPFile::CheckSignature(const SecureBinaryData &data, const SecureBinaryData &signature, unsigned int keyIndex
   , const SecureBinaryData &password)
{
   if (keyIndex < GetUsedKeysCount()) {
      logger_->error("[OTPFile::CheckSignature] OTP was rewinded: user is at {}, and we are at {}"
         , keyIndex, GetUsedKeysCount());
      return false;
   }
   if (GetUsedKeysCount() < keyIndex) {
      logger_->debug("[OTPFile::CheckSignature] advancing to user key: from {} to {}", GetUsedKeysCount(), keyIndex);
      AdvanceKeysTo(keyIndex, password);
   }

   const auto privKey = AdvanceKeysTo(usedKeysCount_ + 1, password);
   if (privKey.isNull()) {
      logger_->error("[OTPFile::CheckSignature] Failed to get/advance OTP private key");
      return false;
   }

   CryptoECDSA crypto;
   const auto pubKey = crypto.ComputePublicKey(privKey);
   if (!crypto.VerifyData(data, signature, pubKey)) {
      logger_->error("[OTPFile::CheckSignature] signature is invalid");
      return false;
   }
   return true;
}
