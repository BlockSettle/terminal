#include <QFile>
#include <QVariant>
#include <QPixmap>
#include <QStandardPaths>

#include <spdlog/spdlog.h>

#include "CoreWalletsManager.h"
#include "CoreHDWallet.h"
#include "PaperBackupWriter.h"
#include "SignerSettings.h"
#include "WalletBackupFile.h"
#include "WalletEncryption.h"
#include "WalletsProxy.h"
#include "UiUtils.h"


WalletsProxy::WalletsProxy(const std::shared_ptr<spdlog::logger> &logger
   , const std::shared_ptr<bs::core::WalletsManager> &walletsMgr, const std::shared_ptr<SignerSettings> &params)
   : logger_(logger), walletsMgr_(walletsMgr), settings_(params)
{
/*   connect(walletsMgr_.get(), &WalletsManager::walletsReady, this, &WalletsProxy::onWalletsChanged);
   connect(walletsMgr_.get(), &WalletsManager::walletsLoaded, this, &WalletsProxy::onWalletsChanged);*/
   //FIXME: will need to switch to bs::sync::WalletsManager later
}

void WalletsProxy::onWalletsChanged()
{
   walletsLoaded_ = true;
   emit walletsChanged();
}

std::shared_ptr<bs::core::hd::Wallet> WalletsProxy::getRootForId(const QString &walletId) const
{
   auto wallet = walletsMgr_->getHDWalletById(walletId.toStdString());
   if (!wallet) {
      wallet = walletsMgr_->getHDRootForLeaf(walletId.toStdString());
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
   return wallet ? QString::fromStdString(wallet->walletId()) : QString();
}

QString WalletsProxy::getRootWalletName(const QString &walletId) const
{
   const auto &wallet = getRootForId(walletId);
   return wallet ? QString::fromStdString(wallet->name()) : QString();
}

bool WalletsProxy::primaryWalletExists() const
{
   return (walletsMgr_->getPrimaryWallet() != nullptr);
}

bool WalletsProxy::changePassword(const QString &walletId
                                  , bs::wallet::QPasswordData *oldPasswordData
                                  , bs::wallet::QPasswordData *newPasswordData)
{
   const auto wallet = getRootForId(walletId);
   if (!wallet) {
      emit walletError(walletId, tr("Failed to change wallet password: wallet not found"));
      return false;
   }

   bool result = wallet->changePassword({ *newPasswordData }, { 1, 1 }
      , oldPasswordData->password, false, false, false);

   if (!result) {
      emit walletError(walletId, tr("Failed to change wallet password: password is invalid"));
      return false;
   }

//   emit walletsMgr_.get()->walletChanged();   //FIXME
   return true;
}

bool WalletsProxy::addEidDevice(const QString &walletId
                                , bs::wallet::QPasswordData *oldPasswordData
                                , bs::wallet::QPasswordData *newPasswordData)
{
   //   Add new device workflow:
   //   1. decrypt wallet by ActivateWalletOldDevice
   //   2. get new password by ActivateWalletNewDevice
   //   3. call hd::Wallet::changePassword(const std::vector<wallet::PasswordData> &newPass, wallet::KeyRank, const SecureBinaryData &oldPass, bool addNew, bool removeOld, bool dryRun) with following args:
   //    - newPass is vector of single new key
   //    - key rank: get current encryptionRank() and add +1 to .second (total keys)
   //    - oldPass is old password requested form existing device
   //    - addNew = true
   //    - removeOld, dryRun = false

   const auto wallet = getRootForId(walletId);
   if (!wallet) {
      emit walletError(walletId, tr("Failed to change wallet password: wallet not found"));
      return false;
   }

   //bs::hd::Wallet;
   bs::wallet::KeyRank encryptionRank = wallet->encryptionRank();
   encryptionRank.second++;

   bool result = wallet->changePassword({ *newPasswordData }, encryptionRank
      , oldPasswordData->password, true, false, false);

   if (!result) {
      emit walletError(walletId, tr("Failed to add new device"));
      return false;
   }

//   emit walletsMgr_.get()->walletChanged();   //FIXME later
   return true;
}

bool WalletsProxy::removeEidDevice(const QString &walletId, bs::wallet::QPasswordData *oldPasswordData, int removedIndex)
{
   //   Delete device workflow:
   //   1. decrypt wallet by DeactivateWalletDevice
   //   2. call hd::Wallet::changePassword:
   //    - newPass is vector of existing encKeys except key of device which should be deleted
   //    - key rank: get current encryptionRank() and add +1 to .second (total keys)
   //    - oldPass is old password requested form existing device
   //    - addNew, dryRun = false
   //    - removeOld = true

   const auto wallet = getRootForId(walletId);
   if (!wallet) {
      emit walletError(walletId, tr("Failed to change wallet password: wallet not found"));
      return false;
   }

   if (wallet->encryptionKeys().size() == 1) {
      emit walletError(walletId, tr("Failed to remove last device"));
      return false;
   }

   if (wallet->encryptionRank().second == 1) {
      emit walletError(walletId, tr("Failed to remove last device"));
      return false;
   }

   if (removedIndex >= wallet->encryptionKeys().size() || removedIndex < 0) {
      emit walletError(walletId, tr("Failed to remove invalid index"));
      return false;
   }

   bs::wallet::KeyRank encryptionRank = wallet->encryptionRank();
   encryptionRank.second--;

   // remove index from encKeys
   std::vector<bs::wallet::PasswordData> newPasswordData;
   for (int i = 0; i < wallet->encryptionKeys().size(); ++i) {
      if (removedIndex == i) continue;
      bs::wallet::QPasswordData pd;
      pd.encType = bs::wallet::EncryptionType::Auth;
      pd.encKey = wallet->encryptionKeys()[i];
      newPasswordData.push_back(pd);
   }

   bool result = wallet->changePassword(newPasswordData, encryptionRank
      , oldPasswordData->password, false, true, false);

   if (!result) {
      emit walletError(walletId, tr("Failed to add new device"));
      return false;
   }
//   emit walletsMgr_.get()->walletChanged();   //FIXME later
   return true;
}

QString WalletsProxy::getWoWalletFile(const QString &walletId) const
{
   return (QString::fromStdString(bs::hd::Wallet::fileNamePrefix(true)) + walletId + QLatin1String("_wallet.lmdb"));
}

bool WalletsProxy::exportWatchingOnly(const QString &walletId, QString path, bs::wallet::QPasswordData *passwordData) const
{
   const auto wallet = getRootForId(walletId);
   if (!wallet) {
      emit walletError(walletId, tr("Failed to export: wallet not found"));
      return false;
   }
   const auto woWallet = wallet->createWatchingOnly(passwordData->password);
   if (!woWallet) {
      logger_->error("[WalletsProxy] failed to create watching-only wallet for id {}", walletId.toStdString());
      emit walletError(walletId, tr("Failed to create watching-only wallet for %1 (id %2)")
         .arg(QString::fromStdString(wallet->name())).arg(walletId));
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
         .arg(QString::fromStdString(wallet->name())).arg(walletId).arg(path)
         .arg(QLatin1String(e.what())));
   }
   return false;
}

bool WalletsProxy::backupPrivateKey(const QString &walletId
                                    , QString fileName
                                    , bool isPrintable
                                    , bs::wallet::QPasswordData *passwordData) const
{
   const auto wallet = getRootForId(walletId);
   if (!wallet) {
      emit walletError(walletId, tr("Failed to backup private key: wallet not found"));
      return false;
   }
   const auto &decrypted = wallet->getRootNode(passwordData->password);
   if (!decrypted) {
      logger_->error("[WalletsProxy] failed to decrypt root node for {}", walletId.toStdString());
      emit walletError(walletId, tr("Failed to decrypt private key for wallet %1 (id %2)")
         .arg(QString::fromStdString(wallet->name())).arg(walletId));
      return false;
   }

   EasyCoDec::Data easyData, edChainCode;
   try {
      easyData = bs::core::wallet::Seed(NetworkType::Invalid, decrypted->privateKey()).toEasyCodeChecksum();
      if (!decrypted->chainCode().isNull()) {
         edChainCode = bs::core::wallet::Seed(NetworkType::Invalid, decrypted->chainCode()).toEasyCodeChecksum();
      }
   }
   catch (const std::exception &e) {
      logger_->error("[WalletsProxy] failed to encode private key: {}", e.what());
      emit walletError(walletId, tr("Failed to encode private key for wallet %1 (id %2)")
         .arg(QString::fromStdString(wallet->name())).arg(walletId));
      return false;
   }

#if !defined (Q_OS_WIN)
   if (!fileName.startsWith(QLatin1Char('/'))) {
      fileName = QLatin1String("/") + fileName;
   }
#endif

   if (isPrintable) {
      try {
         WalletBackupPdfWriter pdfWriter(QString::fromStdString(wallet->walletId()),
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
            .arg(QString::fromStdString(wallet->name())).arg(walletId));
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
/*      WalletBackupFile backupData(wallet, easyData, edChainCode);
      f.write(QByteArray::fromStdString(backupData.Serialize()));*/
      //FIXME: switch to bs::sync wallet later
   }
   return true;
}

bool WalletsProxy::walletNameExists(const QString &name) const
{
   return walletNames().contains(name);
}

QString WalletsProxy::defaultBackupLocation() const
{
   return QString::fromLatin1("file://") +
      QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
}

bool WalletsProxy::createWallet(bool isPrimary
                                , bs::wallet::QSeed *seed
                                , bs::hd::WalletInfo *walletInfo
                                , bs::wallet::QPasswordData *passwordData)
{
   if (seed->networkType() == bs::wallet::QSeed::Invalid) {
      emit walletError({}, tr("Failed to create wallet with invalid seed"));
      return false;
   }

   try {
      walletsMgr_->createWallet(walletInfo->name().toStdString()
                              , walletInfo->desc().toStdString()
                              , *seed
                              , settings_->getWalletsDir().toStdString()
                              , isPrimary
                              , { *passwordData }
                              , { 1, 1 });
   }
   catch (const std::exception &e) {
      logger_->error("[WalletsProxy] failed to create wallet: {}", e.what());
      emit walletError({}, tr("Failed to create wallet: %1").arg(QLatin1String(e.what())));
      return false;
   }
   return true;
}

bool WalletsProxy::deleteWallet(const QString &walletId)
{
   bool ok = false;
   const auto &rootWallet = walletsMgr_->getHDWalletById(walletId.toStdString());
   if (rootWallet) {
      ok = walletsMgr_->deleteWalletFile(rootWallet);
   }
   // Don't remove leaves?
//   else {
//      const auto wallet = walletsMgr_->GetWalletById(walletId.toStdString());
//      if (wallet) {
//         ok = walletsMgr_->DeleteWalletFile(wallet);
//      }
//   }

   if (!ok) emit walletError(walletId, tr("Failed to find wallet with id %1").arg(walletId));

//   emit walletsMgr_.get()->walletChanged();   //FIXME
   return ok;
}

QStringList WalletsProxy::walletNames() const
{
   QStringList result;
   for (unsigned int i = 0; i < walletsMgr_->getHDWalletsCount(); i++) {
      const auto &wallet = walletsMgr_->getHDWallet(i);
      result.push_back(QString::fromStdString(wallet->name()));
   }
   return result;
}

int WalletsProxy::indexOfWalletId(const QString &walletId) const
{
   for (unsigned int i = 0; i < walletsMgr_->getHDWalletsCount(); i++) {
      const auto &wallet = walletsMgr_->getHDWallet(i);
      if (wallet->walletId() == walletId.toStdString()) {
         return i;
      }
   }
   return 0;
}

QString WalletsProxy::walletIdForIndex(int index) const
{
   const auto &wallet = walletsMgr_->getHDWallet(index);
   if (wallet) {
      return QString::fromStdString(wallet->walletId());
   }
   return {};
}

