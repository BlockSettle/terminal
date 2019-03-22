#include <QFile>
#include <QVariant>
#include <QPixmap>
#include <QStandardPaths>

#include <spdlog/spdlog.h>

#include "CoreHDWallet.h"
#include "PaperBackupWriter.h"
#include "SignerAdapter.h"
#include "SignerSettings.h"
#include "UiUtils.h"
#include "WalletBackupFile.h"
#include "WalletEncryption.h"
#include "WalletsProxy.h"
#include "Wallets/SyncHDWallet.h"
#include "Wallets/SyncWalletsManager.h"


WalletsProxy::WalletsProxy(const std::shared_ptr<spdlog::logger> &logger
   , SignerAdapter *adapter)
   : QObject(nullptr), logger_(logger), adapter_(adapter)
{
   connect(adapter_, &SignerAdapter::ready, this, &WalletsProxy::setWalletsManager);
}

void WalletsProxy::setWalletsManager()
{
   walletsMgr_ = adapter_->getWalletsManager();
   connect(walletsMgr_.get(), &bs::sync::WalletsManager::walletsReady, this, &WalletsProxy::onWalletsChanged);
   connect(walletsMgr_.get(), &bs::sync::WalletsManager::walletsSynchronized, [this] { walletsSynchronized_ = true; });
}

void WalletsProxy::onWalletsChanged()
{
   emit walletsChanged();
}

std::shared_ptr<bs::sync::hd::Wallet> WalletsProxy::getRootForId(const QString &walletId) const
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

void WalletsProxy::changePassword(const QString &walletId
                                  , bs::wallet::QPasswordData *oldPasswordData
                                  , bs::wallet::QPasswordData *newPasswordData)
{
   const auto wallet = getRootForId(walletId);
   if (!wallet) {
      emit walletError(walletId, tr("Failed to change wallet password: wallet not found"));
      return;
   }

   const auto &cbChangePwdResult = [this, walletId](bool result) {
      if (result) {
         emit walletsMgr_.get()->walletChanged();
      }
      else {
         emit walletError(walletId, tr("Failed to change wallet password: password is invalid"));
      }
   };
   wallet->changePassword(cbChangePwdResult, { *newPasswordData }, { 1, 1 }
      , oldPasswordData->password, false, false, false);
}

void WalletsProxy::addEidDevice(const QString &walletId
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
      return;
   }

   //bs::hd::Wallet;
   bs::wallet::KeyRank encryptionRank = wallet->encryptionRank();
   encryptionRank.second++;

   const auto &cbChangePwdResult = [this, walletId](bool result) {
      if (result) {
         emit walletsMgr_.get()->walletChanged();
      }
      else {
         emit walletError(walletId, tr("Failed to add new device"));
      }
   };
   wallet->changePassword(cbChangePwdResult, { *newPasswordData }
      , encryptionRank, oldPasswordData->password, true, false, false);
}

void WalletsProxy::removeEidDevice(const QString &walletId, bs::wallet::QPasswordData *oldPasswordData, int removedIndex)
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
      return;
   }

   if (wallet->encryptionKeys().size() == 1) {
      emit walletError(walletId, tr("Failed to remove last device"));
      return;
   }

   if (wallet->encryptionRank().second == 1) {
      emit walletError(walletId, tr("Failed to remove last device"));
      return;
   }

   if (removedIndex >= wallet->encryptionKeys().size() || removedIndex < 0) {
      emit walletError(walletId, tr("Failed to remove invalid index"));
      return;
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

   const auto &cbChangePwdResult = [this, walletId](bool result) {
      if (result) {
         emit walletsMgr_.get()->walletChanged();
      } else {
         emit walletError(walletId, tr("Failed to delete device"));
      }
   };

   wallet->changePassword(cbChangePwdResult, newPasswordData, encryptionRank
      , oldPasswordData->password, false, true, false);
}

QString WalletsProxy::getWoWalletFile(const QString &walletId) const
{
   return (QString::fromStdString(bs::core::hd::Wallet::fileNamePrefix(true)) + walletId + QLatin1String("_wallet.lmdb"));
}

void WalletsProxy::exportWatchingOnly(const QString &walletId, const QString &path, bs::wallet::QPasswordData *passwordData) const
{
   const auto &cbResult = [this, walletId](bool result) {
      if (!result) {
         logger_->error("[WalletsProxy] failed to create watching-only wallet for id {}", walletId.toStdString());
         emit walletError(walletId, tr("Failed to create watching-only wallet for %1").arg(walletId));
      }
   };
   adapter_->createWatchingOnlyWallet(walletId, passwordData->password, path, cbResult);
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
   const auto &cbResult = [this, fileName, walletId, name=wallet->name(), desc=wallet->description(), isPrintable]
      (const SecureBinaryData &privKey, const SecureBinaryData &chainCode) {
      QString fn = fileName;
      if (privKey.isNull()) {
         logger_->error("[WalletsProxy] failed to get root private key: {}", chainCode.toBinStr());
         emit walletError(walletId, tr("Failed to decrypt private key for wallet %1").arg(walletId));
         return;
      }

      EasyCoDec::Data easyData, edChainCode;
      try {
         easyData = bs::core::wallet::Seed(NetworkType::Invalid, privKey).toEasyCodeChecksum();
         if (!chainCode.isNull()) {
            edChainCode = bs::core::wallet::Seed(NetworkType::Invalid, chainCode).toEasyCodeChecksum();
         }
      } catch (const std::exception &e) {
         logger_->error("[WalletsProxy] failed to encode private key: {}", e.what());
         emit walletError(walletId, tr("Failed to encode private key for wallet %1").arg(walletId));
         return;
      }

#if !defined (Q_OS_WIN)
      if (!fileName.startsWith(QLatin1Char('/'))) {
         fn = QLatin1String("/") + fn;
      }
#endif
      if (isPrintable) {
         try {
            WalletBackupPdfWriter pdfWriter(walletId, QString::fromStdString(easyData.part1),
               QString::fromStdString(easyData.part2), QPixmap(QLatin1String(":/FULL_LOGO")),
               UiUtils::getQRCode(QString::fromStdString(easyData.part1 + "\n" + easyData.part2)));
            if (!pdfWriter.write(fn)) {
               throw std::runtime_error("write failure");
            }
         } catch (const std::exception &e) {
            logger_->error("[WalletsProxy] failed to output PDF: {}", e.what());
            emit walletError(walletId, tr("Failed to output PDF with private key backup for wallet %1")
               .arg(walletId));
            return;
         }
      } else {
         QFile f(fn);
         if (!f.open(QIODevice::WriteOnly)) {
            logger_->error("[WalletsProxy] failed to open file {} for writing", fn.toStdString());
            emit walletError(walletId, tr("Failed to open digital wallet backup file {} for writing").arg(fn));
            return;
         }
         WalletBackupFile backupData(walletId.toStdString(), name, desc, easyData, edChainCode);
         f.write(QByteArray::fromStdString(backupData.Serialize()));
      }
   };
   adapter_->getDecryptedRootNode(wallet->walletId(), passwordData->password, cbResult);

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
      ok = walletsMgr_->deleteWallet(rootWallet);
   }

   if (!ok) emit walletError(walletId, tr("Failed to find wallet with id %1").arg(walletId));

   emit walletsMgr_.get()->walletChanged();
   return ok;
}

QStringList WalletsProxy::walletNames() const
{
   QStringList result;
   for (unsigned int i = 0; i < walletsMgr_->hdWalletsCount(); i++) {
      const auto &wallet = walletsMgr_->getHDWallet(i);
      result.push_back(QString::fromStdString(wallet->name()));
   }
   return result;
}

int WalletsProxy::indexOfWalletId(const QString &walletId) const
{
   for (unsigned int i = 0; i < walletsMgr_->hdWalletsCount(); i++) {
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
