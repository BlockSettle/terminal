#include <QFile>
#include <QVariant>
#include <QPixmap>

#include <spdlog/spdlog.h>

#include "HDWallet.h"
#include "PaperBackupWriter.h"
#include "SignerSettings.h"
#include "WalletBackupFile.h"
#include "WalletEncryption.h"
#include "WalletsManager.h"
#include "WalletsProxy.h"
#include "UiUtils.h"


WalletsProxy::WalletsProxy(const std::shared_ptr<spdlog::logger> &logger
   , const std::shared_ptr<WalletsManager> &walletsMgr, const std::shared_ptr<SignerSettings> &params)
   : logger_(logger), walletsMgr_(walletsMgr), params_(params)
{
   connect(walletsMgr_.get(), &WalletsManager::walletsReady, this, &WalletsProxy::onWalletsChanged);
   connect(walletsMgr_.get(), &WalletsManager::walletsLoaded, this, &WalletsProxy::onWalletsChanged);
}

void WalletsProxy::onWalletsChanged()
{
   walletsLoaded_ = true;
   emit walletsChanged();
}

std::shared_ptr<bs::hd::Wallet> WalletsProxy::getRootForId(const QString &walletId) const
{
   auto wallet = walletsMgr_->GetHDWalletById(walletId.toStdString());
   if (!wallet) {
      wallet = walletsMgr_->GetHDRootForLeaf(walletId.toStdString());
      if (!wallet) {
         logger_->error("[WalletsProxy] failed to find root wallet with id {}", walletId.toStdString());
         return nullptr;
      }
   }
   return wallet;
}

QString WalletsProxy::getRootWalletId(const QString &walletId) const
{
   const auto &wallet = getRootForId(walletId);
   return wallet ? QString::fromStdString(wallet->getWalletId()) : QString();
}

QString WalletsProxy::getRootWalletName(const QString &walletId) const
{
   const auto &wallet = getRootForId(walletId);
   return wallet ? QString::fromStdString(wallet->getName()) : QString();
}

bool WalletsProxy::primaryWalletExists() const
{
   return walletsMgr_->HasPrimaryWallet();
}

static bs::wallet::EncryptionType mapEncType(WalletInfo::EncryptionType encType)
{
   switch (encType)
   {
   case WalletInfo::EncryptionType::Password:   return bs::wallet::EncryptionType::Password;
   case WalletInfo::EncryptionType::Freja:      return bs::wallet::EncryptionType::Freja;
   case WalletInfo::EncryptionType::Unencrypted:
   default:    return bs::wallet::EncryptionType::Unencrypted;
   }
}

bool WalletsProxy::changePassword(const QString &walletId, const QString &oldPass, const QString &newPass
   , WalletInfo::EncryptionType encType, const QString &encKey)
{
   const auto wallet = getRootForId(walletId);
   if (!wallet) {
      emit walletError(walletId, tr("Failed to change wallet password: wallet not found"));
      return false;
   }
   const bs::wallet::PasswordData pwdData = { BinaryData::CreateFromHex(newPass.toStdString()), mapEncType(encType), encKey.toStdString() };
   if (!wallet->changePassword({ pwdData }, { 1, 1 }, BinaryData::CreateFromHex(oldPass.toStdString()))) {
      emit walletError(walletId, tr("Failed to change wallet password: password is invalid"));
      return false;
   }
   return true;
}

QString WalletsProxy::getWoWalletFile(const QString &walletId) const
{
   return (QString::fromStdString(bs::hd::Wallet::fileNamePrefix(true)) + walletId + QLatin1String("_wallet.lmdb"));
}

bool WalletsProxy::exportWatchingOnly(const QString &walletId, QString path, const QString &password) const
{
   const auto wallet = getRootForId(walletId);
   if (!wallet) {
      emit walletError(walletId, tr("Failed to export: wallet not found"));
      return false;
   }
   const auto woWallet = wallet->CreateWatchingOnly(BinaryData::CreateFromHex(password.toStdString()));
   if (!woWallet) {
      logger_->error("[WalletsProxy] failed to create watching-only wallet for id {}", walletId.toStdString());
      emit walletError(walletId, tr("Failed to create watching-only wallet for %1 (id %2)")
         .arg(QString::fromStdString(wallet->getName())).arg(walletId));
      return false;
   }

#if !defined (Q_OS_WIN)
   if (!path.startsWith(QLatin1Char('/'))) {
      path = QLatin1String("/") + path;
   }
#endif
   try {
      woWallet->saveToDir(path.toStdString());
      return true;
   }
   catch (const std::exception &e) {
      logger_->error("[WalletsProxy] failed to save watching-only wallet to {}: {}", path.toStdString(), e.what());
      emit walletError(walletId, tr("Failed to save watching-only wallet for %1 (id %2) to %3: %4")
         .arg(QString::fromStdString(wallet->getName())).arg(walletId).arg(path)
         .arg(QLatin1String(e.what())));
   }
   return false;
}

bool WalletsProxy::backupPrivateKey(const QString &walletId, QString fileName, bool isPrintable
   , const QString &password) const
{
   const auto wallet = getRootForId(walletId);
   if (!wallet) {
      emit walletError(walletId, tr("Failed to backup private key: wallet not found"));
      return false;
   }
   const auto &decrypted = wallet->getRootNode(BinaryData::CreateFromHex(password.toStdString()));
   if (!decrypted) {
      logger_->error("[WalletsProxy] failed to decrypt root node for {}", walletId.toStdString());
      emit walletError(walletId, tr("Failed to decrypt private key for wallet %1 (id %2)")
         .arg(QString::fromStdString(wallet->getName())).arg(walletId));
      return false;
   }

   EasyCoDec::Data easyData, edChainCode;
   try {
      easyData = bs::wallet::Seed(NetworkType::Invalid, decrypted->privateKey()).toEasyCodeChecksum();
      if (!decrypted->chainCode().isNull()) {
         edChainCode = bs::wallet::Seed(NetworkType::Invalid, decrypted->chainCode()).toEasyCodeChecksum();
      }
   }
   catch (const std::exception &e) {
      logger_->error("[WalletsProxy] failed to encode private key: {}", e.what());
      emit walletError(walletId, tr("Failed to encode private key for wallet %1 (id %2)")
         .arg(QString::fromStdString(wallet->getName())).arg(walletId));
      return false;
   }

#if !defined (Q_OS_WIN)
   if (!fileName.startsWith(QLatin1Char('/'))) {
      fileName = QLatin1String("/") + fileName;
   }
#endif

   if (isPrintable) {
      try {
         WalletBackupPdfWriter pdfWriter(QString::fromStdString(wallet->getName()),
            QString::fromStdString(wallet->getWalletId()),
            QString::fromStdString(easyData.part1),
            QString::fromStdString(easyData.part2),
            QPixmap(QLatin1String(":/FULL_LOGO")),
            UiUtils::getQRCode(QString::fromStdString(easyData.part1 + "\n" + easyData.part2)));
         if (!pdfWriter.write(fileName)) {
            throw std::runtime_error("write failure");
         }
      }
      catch (const std::exception &e) {
         logger_->error("[WalletsProxy] failed to output PDF: {}", e.what());
         emit walletError(walletId, tr("Failed to output PDF with private key backup for wallet %1 (id %2)")
            .arg(QString::fromStdString(wallet->getName())).arg(walletId));
         return false;
      }
   }
   else {
      QFile f(fileName);
      if (!f.open(QIODevice::WriteOnly)) {
         logger_->error("[WalletsProxy] failed to open file {} for writing", fileName.toStdString());
         emit walletError(walletId, tr("Failed to open digital wallet backup file for writing"));
         return false;
      }
      WalletBackupFile backupData(wallet, easyData, edChainCode);
      f.write(QByteArray::fromStdString(backupData.Serialize()));
   }
   return true;
}

WalletSeed *WalletsProxy::createWalletSeed() const
{
   auto result = new WalletSeed(params_->netType(), (QObject *)this);
   return result;
}

bool WalletsProxy::createWallet(bool isPrimary, const QString &password, WalletSeed *seed)
{
   if (!seed) {
      emit walletError({}, tr("Failed to get wallet seed"));
      return false;
   }
   try {    //!
      const std::vector<bs::wallet::PasswordData> pwdData = { { BinaryData::CreateFromHex(password.toStdString())
         , bs::wallet::EncryptionType::Password, {} } };
      walletsMgr_->CreateWallet(seed->walletName().toStdString(), seed->walletDesc().toStdString()
         , seed->seed(), params_->getWalletsDir(), isPrimary, pwdData, { 1, 1 });
   }
   catch (const std::exception &e) {
      logger_->error("[WalletsProxy] failed to create wallet: {}", e.what());
      emit walletError({}, tr("Failed to create wallet: %1").arg(QLatin1String(e.what())));
      return false;
   }
   return true;
}

bool WalletsProxy::importWallet(bool isPrimary, WalletSeed *seed, const QString &password)
{
   try { //!
      const std::vector<bs::wallet::PasswordData> pwdData = { { BinaryData::CreateFromHex(password.toStdString())
         , bs::wallet::EncryptionType::Password,{} } };
      walletsMgr_->CreateWallet(seed->walletName().toStdString(), seed->walletDesc().toStdString()
         , seed->seed(), params_->getWalletsDir(), isPrimary, pwdData, { 1, 1 });
   }
   catch (const std::exception &e) {
      logger_->error("[WalletsProxy] failed to import wallet: {}", e.what());
      emit walletError({}, tr("Failed to import wallet: %1").arg(QLatin1String(e.what())));
      return false;
   }
   return true;
}

bool WalletsProxy::deleteWallet(const QString &walletId)
{
   const auto &rootWallet = walletsMgr_->GetHDWalletById(walletId.toStdString());
   if (rootWallet) {
      return walletsMgr_->DeleteWalletFile(rootWallet);
   }
   const auto wallet = walletsMgr_->GetWalletById(walletId.toStdString());
   if (wallet) {
      return walletsMgr_->DeleteWalletFile(wallet);
   }
   emit walletError(walletId, tr("Failed to find wallet with id %1").arg(walletId));
   return false;
}

QStringList WalletsProxy::walletNames() const
{
   QStringList result;
   for (unsigned int i = 0; i < walletsMgr_->GetHDWalletsCount(); i++) {
      const auto &wallet = walletsMgr_->GetHDWallet(i);
      result.push_back(QString::fromStdString(wallet->getName()));
   }
   return result;
}

int WalletsProxy::indexOfWalletId(const QString &walletId) const
{
   for (unsigned int i = 0; i < walletsMgr_->GetHDWalletsCount(); i++) {
      const auto &wallet = walletsMgr_->GetHDWallet(i);
      if (wallet->getWalletId() == walletId.toStdString()) {
         return i;
      }
   }
   return 0;
}

QString WalletsProxy::walletIdForIndex(int index) const
{
   const auto &wallet = walletsMgr_->GetHDWallet(index);
   if (wallet) {
      return QString::fromStdString(wallet->getWalletId());
   }
   return {};
}


void WalletSeed::setRandomKey()
{
   seed_.setPrivateKey(SecureBinaryData().GenerateRandom(32));
}

bool WalletSeed::parseDigitalBackupFile(const QString &filename)
{
   QFile file(filename);
   if (!file.exists()) {
      emit error(tr("Digital Backup file %1 doesn't exist").arg(filename));
      return false;
   }
   if (file.open(QIODevice::ReadOnly)) {
      QByteArray data = file.readAll();
      const auto wdb = WalletBackupFile::Deserialize(std::string(data.data(), data.size()));
      if (wdb.id.empty()) {
         emit error(tr("Digital Backup file %1 corrupted").arg(filename));
         return false;
      }
      else {
         seed_ = bs::wallet::Seed::fromEasyCodeChecksum(wdb.seed, wdb.chainCode
            , (seed_.networkType() == NetworkType::Invalid) ? NetworkType::TestNet : seed_.networkType());
         walletName_ = QString::fromStdString(wdb.name);
         walletDesc_ = QString::fromStdString(wdb.description);
      }
   }
   else {
      emit error(tr("Failed to read Digital Backup file %1").arg(filename));
      return false;
   }
   emit seedChanged();
   return true;
}

bool WalletSeed::parsePaperKey(const QString &key)
{
   try {
      const auto seedLines = key.split(QLatin1String("\n"), QString::SkipEmptyParts);
      if (seedLines.count() == 2) {
         EasyCoDec::Data easyData = { seedLines[0].toStdString(), seedLines[1].toStdString() };
         seed_ = bs::wallet::Seed::fromEasyCodeChecksum(easyData, seed_.networkType());
      }
      else if (seedLines.count() == 4) {
         EasyCoDec::Data easyData = { seedLines[0].toStdString(), seedLines[1].toStdString() };
         EasyCoDec::Data edChainCode = { seedLines[2].toStdString(), seedLines[3].toStdString() };
         seed_ = bs::wallet::Seed::fromEasyCodeChecksum(easyData, edChainCode, seed_.networkType());
      }
      else {
         seed_ = { key.toStdString(), seed_.networkType() };
      }
   }
   catch (const std::exception &e) {
      emit error(tr("Failed to parse wallet key: %1").arg(QLatin1String(e.what())));
      return false;
   }
   emit seedChanged();
   return true;
}

QString WalletSeed::walletId() const
{
   return QString::fromStdString(bs::hd::Node(seed_).getId());
}
