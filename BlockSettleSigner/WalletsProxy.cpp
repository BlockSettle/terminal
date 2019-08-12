#include <QFile>
#include <QFileInfo>
#include <QVariant>
#include <QPixmap>
#include <QStandardPaths>
#include <QDir>
#include <QMetaMethod>
#include <QTemporaryDir>

#include <spdlog/spdlog.h>

#include "CoreHDWallet.h"
#include "PaperBackupWriter.h"
#include "SignerAdapter.h"
#include "UiUtils.h"
#include "WalletBackupFile.h"
#include "WalletEncryption.h"
#include "WalletsProxy.h"
#include "Wallets/SyncHDWallet.h"
#include "Wallets/SyncWalletsManager.h"
#include "BSErrorCodeStrings.h"
#include "OfflineSigner.h"
#include "TXInfo.h"

#include "signer.pb.h"

#include <memory>

using namespace Blocksettle;

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
   connect(walletsMgr_.get(), &bs::sync::WalletsManager::walletsSynchronized, [this] {
      walletsSynchronized_ = true;
      onWalletsChanged();
   });
}

void WalletsProxy::onWalletsChanged()
{
   // thread safe replacement of
   // emit walletsChanged();
   QMetaMethod walletsChangedSignal = QMetaMethod::fromSignal(&WalletsProxy::walletsChanged);
   walletsChangedSignal.invoke(this, Qt::QueuedConnection);
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
   return (walletsMgr_ && (walletsMgr_->getPrimaryWallet() != nullptr));
}

void WalletsProxy::changePassword(const QString &walletId
                                  , bs::wallet::QPasswordData *oldPasswordData
                                  , bs::wallet::QPasswordData *newPasswordData
                                  , const QJSValue &jsCallback)
{
   const auto wallet = getRootForId(walletId);
   if (!wallet) {
      emit walletError(walletId, tr("Failed to change wallet password: wallet not found"));
      return;
   }

   const auto &cbChangePwdResult = [this, walletId, jsCallback](bool result) {
      QJSValueList args;
      args << QJSValue(result);

      QMetaObject::invokeMethod(this, "invokeJsCallBack", Qt::QueuedConnection
                                , Q_ARG(QJSValue, jsCallback)
                                , Q_ARG(QJSValueList, args));

      if (result) {
         onWalletsChanged();
      }
   };

   adapter_->changePassword(walletId.toStdString(), { *newPasswordData }, { 1, 1 }
                          , oldPasswordData->password, false, false, false, cbChangePwdResult);
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
         emit walletsMgr_->walletChanged(walletId.toStdString());
      }
      else {
         emit walletError(walletId, tr("Failed to add new device"));
      }
   };
   adapter_->changePassword(walletId.toStdString(), { *newPasswordData }
      , encryptionRank, oldPasswordData->password, true, false, false, cbChangePwdResult);
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
         emit walletsMgr_->walletChanged(walletId.toStdString());
      } else {
         emit walletError(walletId, tr("Failed to delete device"));
      }
   };

   adapter_->changePassword(walletId.toStdString(), newPasswordData, encryptionRank
      , oldPasswordData->password, false, true, false, cbChangePwdResult);
}

QString WalletsProxy::getWoWalletFile(const QString &walletId) const
{
   return (QString::fromStdString(bs::core::hd::Wallet::fileNamePrefix(true)) + walletId + QLatin1String("_wallet.lmdb"));
}

void WalletsProxy::exportWatchingOnly(const QString &walletId, const QString &filePath
   , bs::wallet::QPasswordData *passwordData, const QJSValue &jsCallback)
{
   logger_->debug("[{}] path={}", __func__, filePath.toStdString());
   const auto &cbResult = [this, walletId, filePath, jsCallback, passwordData](const SecureBinaryData &privKey, const SecureBinaryData &seedData) {
      std::shared_ptr<bs::core::hd::Wallet> newWallet;
      try {
         const auto hdWallet = walletsMgr_->getHDWalletById(walletId.toStdString());
         if (!hdWallet) {
            throw std::runtime_error("failed to find wallet with id " + walletId.toStdString());
         }

         // New wallets currently do not allow select file name used to save.
         // Save WO wallet to the temporary dir and move out where it should be.
         // Temporary directory will be removed when goes out of scope.
         QTemporaryDir tmpDir;
         if (!tmpDir.isValid()) {
            throw std::runtime_error("failed to create temporary dir");
         }
         SPDLOG_LOGGER_DEBUG(logger_, "temporary export WO file to {}", tmpDir.path().toStdString());

         const bs::core::wallet::Seed seed(seedData, hdWallet->networkType());
         newWallet = std::make_shared<bs::core::hd::Wallet>(hdWallet->name(), hdWallet->description()
            , seed, passwordData->password, tmpDir.path().toStdString());

         for (const auto &group : hdWallet->getGroups()) {
            auto newGroup = newWallet->createGroup(static_cast<bs::hd::CoinType>(group->index()));
            if (!newGroup) {
               throw std::runtime_error("failed to create group");
            }
            auto lock = newWallet->lockForEncryption(passwordData->password);
            for (const auto &leaf : group->getLeaves()) {
               try {
                  auto newLeaf = newGroup->createLeaf(leaf->index());
                  if (!newLeaf) {
                     throw std::runtime_error("uncreatable");
                  }
                  for (const auto &addr : leaf->getExtAddressList()) {
                     newLeaf->getNewExtAddress(addr.getType());
                  }
                  for (const auto &addr : leaf->getIntAddressList()) {
                     newLeaf->getNewIntAddress(addr.getType());
                  }
                  logger_->debug("[WalletsProxy::exportWatchingOnly] leaf {} has {} + {} addresses"
                     , newLeaf->walletId(), newLeaf->getExtAddressCount(), newLeaf->getIntAddressCount());
               }
               catch (const std::exception &e) {
                  logger_->warn("[WalletsProxy::exportWatchingOnly] WO leaf {} ({}/{}) not created: {}"
                     , leaf->walletId(), group->index(), leaf->index(), e.what());
               }
            }
         }

         // Do not keep WO wallet ptr here as it would lock file
         if (newWallet->createWatchingOnly() == nullptr) {
             throw std::runtime_error("can't create WO wallet");
         }

         newWallet->eraseFile();
         // Clear pointer to not call one more time eraseFile if something would fail below
         newWallet = nullptr;

         QDir dir(tmpDir.path());
         auto entryList = dir.entryList({QStringLiteral("*.lmdb")});
         if (entryList.empty()) {
            throw std::runtime_error("export failed (can't find exported file)");
         }

         if (entryList.size() != 1) {
            throw std::runtime_error("export failed (too many exported files)");
         }

         if (QFile::exists(filePath)) {
            bool result = QFile::remove(filePath);
            if (!result) {
               throw std::runtime_error("can't delete old file");
            }
         }

         bool result = QFile::rename(dir.filePath(entryList[0]), filePath);
         if (!result) {
            throw std::runtime_error("write failed");
         }
      }
      catch (const std::exception &e) {
         if (newWallet) {
            newWallet->eraseFile();
         }

         logger_->error("[WalletsProxy::exportWatchingOnly] {}", e.what());
         QMetaObject::invokeMethod(this, [this, jsCallback, walletId, filePath, errorMessage = e.what()] {
            QJSValueList args;
            QString message = tr("Failed to save watching-only wallet for %1 to %2: %3")
                  .arg(walletId).arg(filePath).arg(QString::fromStdString(errorMessage));
            args << QJSValue(false) << message;
            invokeJsCallBack(jsCallback, args);
         });
         return;
      }

      QMetaObject::invokeMethod(this, [this, jsCallback] {
         QJSValueList args;
         args << QJSValue(true) << QStringLiteral("");
         invokeJsCallBack(jsCallback, args);
      });
   };
   adapter_->createWatchingOnlyWallet(walletId, passwordData->password, cbResult);
}

bool WalletsProxy::backupPrivateKey(const QString &walletId, QString fileName, bool isPrintable
   , bs::wallet::QPasswordData *passwordData, const QJSValue &jsCallback)
{
   const auto wallet = getRootForId(walletId);
   if (!wallet) {
      emit walletError(walletId, tr("Failed to backup private key: wallet not found"));
      return false;
   }
   const auto &cbResult = [this, fileName, walletId, name=wallet->name(), desc=wallet->description(), isPrintable, jsCallback]
      (const SecureBinaryData &privKey, const SecureBinaryData &chainCode) {
      QString fn = fileName;

      std::string privKeyString;
      EasyCoDec::Data seedData;
      try {
         const auto wallet = walletsMgr_->getHDWalletById(walletId.toStdString());
         if (!wallet) {
            throw std::runtime_error("failed to find wallet with id " + walletId.toStdString());
         }
         seedData = bs::core::wallet::Seed(chainCode, wallet->networkType()).toEasyCodeChecksum();
         privKeyString = privKey.toBinStr();
      } catch (const std::exception &e) {
         logger_->error("[WalletsProxy] failed to encode private key: {}", e.what());
         emit walletError(walletId, tr("Failed to encode private key for wallet %1").arg(walletId));
         return;
      }

      if (isPrintable) {
         try {
            WalletBackupPdfWriter pdfWriter(walletId, QString::fromStdString(seedData.part1),
               QString::fromStdString(seedData.part2), QPixmap(QLatin1String(":/FULL_LOGO")),
               UiUtils::getQRCode(QString::fromStdString(seedData.part1 + "\n" + seedData.part2)));
            if (!pdfWriter.write(fn)) {
               throw std::runtime_error("write failure");
            }
         } catch (const std::exception &e) {
            logger_->error("[WalletsProxy] failed to output PDF: {}", e.what());
            QMetaObject::invokeMethod(this, [this, jsCallback, walletId] {
               QJSValueList args;
               args << QJSValue(false) << tr("Failed to output PDF with private key backup for wallet %1")
                       .arg(walletId);
               invokeJsCallBack(jsCallback, args);
            });
            return;
         }
      } else {
         QFile f(fn);
         if (!f.open(QIODevice::WriteOnly)) {
            logger_->error("[WalletsProxy] failed to open file {} for writing", fn.toStdString());
            QMetaObject::invokeMethod(this, [this, jsCallback, walletId, fn] {
               QJSValueList args;
               args << QJSValue(false) << tr("Failed to open digital wallet backup file %1 for writing").arg(fn);
               invokeJsCallBack(jsCallback, args);
            });
            return;
         }
         WalletBackupFile backupData(walletId.toStdString(), name, desc, seedData, privKeyString);
         f.write(QByteArray::fromStdString(backupData.Serialize()));
      }
      QMetaObject::invokeMethod(this, [this, jsCallback] {
         QJSValueList args;
         args << QJSValue(true) << QStringLiteral("");
         invokeJsCallBack(jsCallback, args);
      });
   };
   adapter_->getDecryptedRootNode(wallet->walletId(), passwordData->password, cbResult);

   return true;
}

void WalletsProxy::signOfflineTx(const QString &fileName, const QJSValue &jsCallback)
{
   logger_->debug("Processing file {}...", fileName.toStdString());
   QFile file(fileName);
   if (!file.exists()) {
      invokeJsCallBack(jsCallback, QJSValueList() << QJSValue(false) << tr("File %1 doesn't exist").arg(fileName));
      return;
   }

   if (!file.open(QIODevice::ReadOnly)) {
      invokeJsCallBack(jsCallback, QJSValueList() << QJSValue(false) << tr("Failed to open %1 for reading").arg(fileName));
      return;
   }

   const auto &data = file.readAll().toStdString();
   if (data.empty()) {
      invokeJsCallBack(jsCallback, QJSValueList() << QJSValue(false) << tr("File %1 contains no data").arg(fileName));
      return;
   }

   const auto &parsedReqs = ParseOfflineTXFile(data);
   if (parsedReqs.empty()) {
      invokeJsCallBack(jsCallback, QJSValueList() << QJSValue(false) << tr("File %1 contains no TX sign requests").arg(fileName));
      return;
   }

   // sort reqs by wallets
   const auto &parsedReqsForWallets = std::make_shared<std::unordered_map<std::string, std::vector<bs::core::wallet::TXSignRequest>>>(); // <wallet_id, reqList>
   const auto walletsMgr = adapter_->getWalletsManager();
   for (const auto &req : parsedReqs) {
      if (!req.prevStates.empty()) {
         invokeJsCallBack(jsCallback, QJSValueList() << QJSValue(false) << tr("Transaction already signed"));
         return;
      }
      const auto &wallet = walletsMgr->getWalletById(req.walletId);
      if (!wallet) {
         invokeJsCallBack(jsCallback, QJSValueList() << QJSValue(false) << tr("Failed to find wallet with ID %1").arg(QString::fromStdString(req.walletId)));
         return;
      }

      parsedReqsForWallets->operator[](req.walletId).push_back(req);
   }

   // sign reqs by wallets
   const auto &requestCbs = std::make_shared<std::vector<std::function<void()>>>();

   for (const auto req : *parsedReqsForWallets) {
      const auto &walletCb = [this, fileName, jsCallback, requestCbs, walletId=req.first, reqs=req.second]() {

         const auto &cb = new bs::signer::QmlCallback<int, QString, bs::wallet::QPasswordData *>
               ([this, fileName, jsCallback, requestCbs, walletId, reqs](int result, const QString &, bs::wallet::QPasswordData *passwordData){

            auto errorCode = static_cast<bs::error::ErrorCode>(result);
            if (errorCode == bs::error::ErrorCode::TxCanceled) {
               return;
            }
            else {
               const auto &cbSigned = [this, fileName, jsCallback, requestCbs, walletId, reqs] (bs::error::ErrorCode result, const BinaryData &signedTX) {
                  if (result != bs::error::ErrorCode::NoError) {
                     invokeJsCallBack(jsCallback, QJSValueList() << QJSValue(false) << tr("Failed to sign request, error code: %1, file: %2")
                                      .arg(static_cast<int>(result))
                                      .arg(fileName));
                     return;
                  }
                  QFileInfo fi(fileName);
                  QString outputFN = fi.path() + QLatin1String("/") + fi.baseName() + QLatin1String("_signed.bin");
                  QFile f(outputFN);
                  if (f.exists()) {
                     invokeJsCallBack(jsCallback, QJSValueList() << QJSValue(false) << tr("File %1 already exists").arg(outputFN));
                     return;
                  }
                  if (!f.open(QIODevice::WriteOnly)) {
                     invokeJsCallBack(jsCallback, QJSValueList() << QJSValue(false) << tr("Failed to open %1 for writing").arg(outputFN));
                     return;
                  }

                  Storage::Signer::SignedTX response;
                  response.set_transaction(signedTX.toBinStr());
                  response.set_comment(reqs[0].comment);

                  Storage::Signer::File fileContainer;
                  auto container = fileContainer.add_payload();
                  container->set_type(Storage::Signer::SignedTXFileType);
                  container->set_data(response.SerializeAsString());

                  const auto data = QByteArray::fromStdString(fileContainer.SerializeAsString());
                  if (f.write(data) != data.size()) {
                     invokeJsCallBack(jsCallback, QJSValueList() << QJSValue(false) << tr("Failed to write TX response data to %1").arg(outputFN));
                     return;
                  }

                  logger_->info("Created signed TX response file in {}", outputFN.toStdString());
                  if (requestCbs->empty()) {
                     // remove original request file?
                     invokeJsCallBack(jsCallback, QJSValueList() << QJSValue(true) << tr("Signed TX saved to %1").arg(outputFN));
                  }
                  else {
                     // run next dialog for next wallet
                     auto fn = std::move(requestCbs->back());
                     requestCbs->pop_back();
                     fn();
                  }
               };
               adapter_->signOfflineTxRequest(reqs[0], passwordData->binaryPassword(), cbSigned);
            }
         });


         // TODO: send to qml list of txInfo
         bs::wallet::TXInfo *txInfo = new bs::wallet::TXInfo(reqs[0]);
         QQmlEngine::setObjectOwnership(txInfo, QQmlEngine::JavaScriptOwnership);

         bs::sync::PasswordDialogData *dialogData = new bs::sync::PasswordDialogData();
         QQmlEngine::setObjectOwnership(dialogData, QQmlEngine::JavaScriptOwnership);

         bs::hd::WalletInfo *walletInfo = adapter_->qmlFactory()->createWalletInfo(walletId);

         adapter_->qmlBridge()->invokeQmlMethod("createTxSignDialog", cb
                                                , tr("Sign Offline TX")
                                                , QVariant::fromValue(txInfo)
                                                , QVariant::fromValue(dialogData)
                                                , QVariant::fromValue(walletInfo));
      };

      requestCbs->push_back(walletCb);
   }

   // run first cb
   auto fn = std::move(requestCbs->back());
   requestCbs->pop_back();
   fn();
}

bool WalletsProxy::walletNameExists(const QString &name) const
{
   return walletsMgr_->walletNameExists(name.toStdString());
}

QString WalletsProxy::generateNextWalletName() const
{
   QString newWalletName;
   size_t nextNumber = walletsMgr_->hdWalletsCount() + 1;
   do {
      newWalletName = tr("Wallet #%1").arg(nextNumber);
      nextNumber++;
   } while (walletNameExists(newWalletName));
   return newWalletName;
}

bool WalletsProxy::isWatchingOnlyWallet(const QString &walletId) const
{
   return walletsMgr_->isWatchingOnly(walletId.toStdString());
}

QString WalletsProxy::defaultBackupLocation() const
{
   return QString::fromLatin1("file://") +
      QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
}

void WalletsProxy::createWallet(bool isPrimary
                                , bs::wallet::QSeed *seed
                                , bs::hd::WalletInfo *walletInfo
                                , bs::wallet::QPasswordData *passwordData
                                , const QJSValue &jsCallback)
{
   if (!walletsMgr_) {
      emit walletError({}, tr("Wallets manager is missing"));
      return;
   }
   if (seed->networkType() == bs::wallet::QSeed::Invalid) {
      emit walletError({}, tr("Failed to create wallet with invalid seed"));
      return;
   }

   auto cb = [this, jsCallback] (bs::error::ErrorCode errorCode) {
      QMetaObject::invokeMethod(this, [this, errorCode, jsCallback] {
         QJSValueList args;
         args << QJSValue(errorCode == bs::error::ErrorCode::NoError)
              << bs::error::ErrorCodeToString(errorCode);
         invokeJsCallBack(jsCallback, args);
      });
   };

   adapter_->createWallet(walletInfo->name().toStdString(), walletInfo->desc().toStdString()
      , *seed, isPrimary, { *passwordData }, { 1, 1 }, cb);
}

void WalletsProxy::deleteWallet(const QString &walletId, const QJSValue &jsCallback)
{
   auto cb = [this, walletId, jsCallback] (bool success, const std::string &error) {
      QJSValueList args;
      args << QJSValue(success) << QString::fromStdString(error);
      QMetaObject::invokeMethod(this, [this, args, jsCallback] {
         invokeJsCallBack(jsCallback, args);
      });
   };
   adapter_->deleteWallet(walletId.toStdString(), cb);
}

std::shared_ptr<bs::sync::hd::Wallet> WalletsProxy::getWoSyncWallet(const bs::sync::WatchingOnlyWallet &wo) const
{
   try {
      auto result = std::make_shared<bs::sync::hd::Wallet>(wo.id, wo.name, wo.description, logger_);
      for (const auto &groupEntry : wo.groups) {
         auto group = result->createGroup(static_cast<bs::hd::CoinType>(groupEntry.type), false);
         for (const auto &leafEntry : groupEntry.leaves) {
            group->createLeaf(leafEntry.index, leafEntry.id);
         }
      }
      return result;
   } catch (const std::exception &e) {
      logger_->error("[WalletsProxy] WO-wallet creation failed: {}", e.what());
   }
   return nullptr;
}

void WalletsProxy::importWoWallet(const QString &walletPath, const QJSValue &jsCallback)
{
   auto cb = [this, jsCallback](const bs::sync::WatchingOnlyWallet &wo) {
      QMetaObject::invokeMethod(this, [this, wo, jsCallback] {
         logger_->debug("imported WO wallet with id {}", wo.id);
         walletsMgr_->adoptNewWallet(getWoSyncWallet(wo));
         QJSValueList args;
         args << QJSValue(wo.id.empty() ? false : true)
            << QString::fromStdString(wo.id.empty() ? wo.description : wo.id);
         invokeJsCallBack(jsCallback, args);
      });
   };

   bs::sync::WatchingOnlyWallet errWallet;
   QFile f(walletPath);
   if (!f.exists()) {
      errWallet.description = "file doesn't exist";
      cb(errWallet);
      return;
   }
   if (!f.open(QIODevice::ReadOnly)) {
      errWallet.description = "failed to open file for reading";
      cb(errWallet);
      return;
   }
   const BinaryData content(f.readAll().toStdString());
   f.close();

   QFileInfo fi(walletPath);

   adapter_->importWoWallet(fi.fileName().toStdString(), content, cb);
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

QJSValue WalletsProxy::invokeJsCallBack(QJSValue jsCallback, QJSValueList args)
{
   if (jsCallback.isCallable()) {
      return jsCallback.call(args);
   }
   else {
      return QJSValue();
   }
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
