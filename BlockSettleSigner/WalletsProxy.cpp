/*

***********************************************************************************
* Copyright (C) 2016 - , BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
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
#include "SignerAdapterContainer.h"
#include "TXInfo.h"
#include "UiUtils.h"
#include "WalletBackupFile.h"
#include "WalletEncryption.h"
#include "WalletsProxy.h"
#include "Wallets/SyncHDWallet.h"
#include "Wallets/SyncWalletsManager.h"
#include "BSErrorCodeStrings.h"
#include "OfflineSigner.h"

#include "signer.pb.h"

#include <memory>

using namespace Blocksettle;

WalletsProxy::WalletsProxy(const std::shared_ptr<spdlog::logger> &logger
   , SignerAdapter *adapter)
   : QObject(nullptr), logger_(logger), adapter_(adapter), signContainer_(adapter->signContainer())
{
   connect(adapter_, &SignerAdapter::ready, this, &WalletsProxy::setWalletsManager);
   connect(adapter_, &SignerAdapter::ccInfoReceived, [this](bool result) {
      hasCCInfo_ = result;
      emit ccInfoChanged();
   });
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
   , bs::wallet::QPasswordData *oldPasswordData, bs::wallet::QPasswordData *newPasswordData
   , const QJSValue &jsCallback)
{
   const auto wallet = getRootForId(walletId);
   const auto &cbChangePwdResult = createChangePwdResultCb(walletId, jsCallback);

   if (!wallet) {
      cbChangePwdResult(bs::error::ErrorCode::WalletNotFound);
      return;
   }

   adapter_->changePassword(walletId.toStdString(), { *newPasswordData }
      , *oldPasswordData, false, false, cbChangePwdResult);
}

void WalletsProxy::addEidDevice(const QString &walletId
   , bs::wallet::QPasswordData *oldPasswordData, bs::wallet::QPasswordData *newPasswordData
   , const QJSValue &jsCallback)
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
   const auto &cbChangePwdResult = createChangePwdResultCb(walletId, jsCallback);

   if (!wallet) {
      cbChangePwdResult(bs::error::ErrorCode::WalletNotFound);
      return;
   }

   adapter_->changePassword(walletId.toStdString(), { *newPasswordData }
      , *oldPasswordData, true, false, cbChangePwdResult);
}

void WalletsProxy::removeEidDevice(const QString &walletId, bs::wallet::QPasswordData *oldPasswordData
   , int removedIndex, const QJSValue &jsCallback)
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
   const auto &cbChangePwdResult = createChangePwdResultCb(walletId, jsCallback);

   if (!wallet) {
      cbChangePwdResult(bs::error::ErrorCode::WalletNotFound);
      return;
   }

   if (wallet->encryptionKeys().size() == 1) {
      cbChangePwdResult(bs::error::ErrorCode::WalletFailedRemoveLastEidDevice);
      return;
   }

   if (wallet->encryptionRank().n == 1) {
      cbChangePwdResult(bs::error::ErrorCode::WalletFailedRemoveLastEidDevice);
      return;
   }

   if (removedIndex >= wallet->encryptionKeys().size() || removedIndex < 0) {
      SPDLOG_LOGGER_ERROR(logger_, "Failed to remove eid device with invalid index: {} of {}", removedIndex, wallet->encryptionKeys().size());
      cbChangePwdResult(bs::error::ErrorCode::InternalError);
      return;
   }

   // remove index from encKeys
   std::vector<bs::wallet::PasswordData> newPasswordData;
   for (int i = 0; i < wallet->encryptionKeys().size(); ++i) {
      if (removedIndex == i) continue;
      bs::wallet::QPasswordData pd;
      pd.metaData = { bs::wallet::EncryptionType::Auth, wallet->encryptionKeys()[i] };
      newPasswordData.push_back(pd);
   }

   adapter_->changePassword(walletId.toStdString(), newPasswordData
      , *oldPasswordData, false, true, cbChangePwdResult);
}

QString WalletsProxy::getWoWalletFile(const QString &walletId) const
{
   return (QString::fromStdString(bs::core::hd::Wallet::fileNamePrefix(true)) + walletId + QLatin1String("_wallet.lmdb"));
}

void WalletsProxy::exportWatchingOnly(const QString &walletId, const QString &filePath
   , bs::wallet::QPasswordData *passwordData, const QJSValue &jsCallback)
{
   auto successCallback = [this, jsCallback, walletId, filePath] {
      SPDLOG_LOGGER_DEBUG(logger_, "WO export succeed");
      QMetaObject::invokeMethod(this, [this, jsCallback] {
         QJSValueList args;
         args << QJSValue(true) << QStringLiteral("");
         invokeJsCallBack(jsCallback, args);
      });
   };

   auto failCallback = [this, jsCallback, walletId, filePath](const std::string &errorMsg) {
      SPDLOG_LOGGER_ERROR(logger_, "export failed: {}", errorMsg);
      QMetaObject::invokeMethod(this, [this, jsCallback, walletId, filePath, errorMsg] {
         QJSValueList args;
         QString message = tr("Failed to save watching-only wallet for %1 to %2: %3")
               .arg(walletId).arg(filePath).arg(QString::fromStdString(errorMsg));
         args << QJSValue(false) << message;
         invokeJsCallBack(jsCallback, args);
      });
   };

   if (walletsMgr_->isWatchingOnly(walletId.toStdString())) {
      SPDLOG_LOGGER_DEBUG(logger_, "copy WO from WO wallet to '{}'", filePath.toStdString());

      adapter_->exportWoWallet(walletId.toStdString(), [walletId, successCallback, failCallback, filePath](const BinaryData &content) {
         if (content.empty()) {
            failCallback("can't read WO file");
            return;
         }

         QFile f(filePath);
         bool result = f.open(QIODevice::WriteOnly);
         if (!result) {
            failCallback("can't open output file");
            return;
         }

         auto size = f.write(reinterpret_cast<const char*>(content.getPtr()), int(content.getSize()));
         if (size != int(content.getSize())) {
            failCallback("write failed");
            return;
         }

         successCallback();
      });
      return;
   }

   SPDLOG_LOGGER_DEBUG(logger_, "export WO from full wallet to '{}'", filePath.toStdString());
   const auto &cbResult = [this, walletId, filePath, successCallback, failCallback, passwordData](const SecureBinaryData &privKey, const SecureBinaryData &seedData) {
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
            , seed, *passwordData, tmpDir.path().toStdString());

         for (const auto &group : hdWallet->getGroups()) {
            if (group->type() != bs::core::wallet::Type::Bitcoin
                && group->type() != bs::core::wallet::Type::ColorCoin) {
               continue;
            }
            auto newGroup = newWallet->createGroup(static_cast<bs::hd::CoinType>(group->index()));
            if (!newGroup) {
               throw std::runtime_error("failed to create group");
            }
            const bs::core::WalletPasswordScoped lock(newWallet, passwordData->password);
            for (const auto &leaf : group->getLeaves()) {
               try {
                  auto newLeaf = newGroup->createLeaf(leaf->path());
                  if (!newLeaf) {
                     throw std::runtime_error("uncreatable");
                  }
                  for (int i = int(leaf->getExtAddressCount()); i > 0; --i) {
                     newLeaf->getNewExtAddress();
                  }
                  for (int i = int(leaf->getIntAddressCount()); i > 0; --i) {
                     newLeaf->getNewIntAddress();
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
         failCallback(e.what());
         return;
      }

      successCallback();
   };
   adapter_->createWatchingOnlyWallet(walletId, passwordData->password, cbResult);
}

bool WalletsProxy::backupPrivateKey(const QString &walletId, QString fileName, bool isPrintable
   , bs::wallet::QPasswordData *passwordData, const QJSValue &jsCallback)
{
   const auto wallet = getRootForId(walletId);
   if (!wallet) {
      QMetaObject::invokeMethod(this, [this, jsCallback] {
         invokeJsCallBack(jsCallback, QJSValueList() << QJSValue(false) << QJSValue(tr("Wallet not found")));
      });
      return false;
   }

   const auto &cbResult = [this, fileName, walletId, name=wallet->name(), desc=wallet->description(), isPrintable, jsCallback]
      (const SecureBinaryData &privKey, const SecureBinaryData &chainCode) {
      QString fn = fileName;

      if (privKey.empty()) {
         logger_->error("[WalletsProxy] error decrypting private key");
         const auto errText = tr("Failed to decrypt private key for wallet %1").arg(walletId);
         QMetaObject::invokeMethod(this, [this, jsCallback, errText] {
            invokeJsCallBack(jsCallback, QJSValueList() << QJSValue(false) << errText);
         });
         return;
      }

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
         const auto errText = tr("Failed to encode private key for wallet %1").arg(walletId);
         QMetaObject::invokeMethod(this, [this, jsCallback, errText] {
            invokeJsCallBack(jsCallback, QJSValueList() << QJSValue(false) << errText);
         });
         return;
      }

      if (isPrintable) {
         try {
            WalletBackupPdfWriter pdfWriter(walletId, QString::fromStdString(seedData.part1),
               QString::fromStdString(seedData.part2),
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

   const auto &parsedReqs = bs::core::wallet::ParseOfflineTXFile(data);
   if (parsedReqs.empty()) {
      invokeJsCallBack(jsCallback, QJSValueList() << QJSValue(false) << tr("File %1 contains no TX sign requests").arg(fileName));
      return;
   }

   // sort reqs by wallets
   const auto &parsedReqsForWallets = std::make_shared<std::unordered_map<std::string, std::vector<bs::core::wallet::TXSignRequest>>>(); // <wallet_id, reqList>
   //const auto walletsMgr = adapter_->getWalletsManager();
   for (const auto &req : parsedReqs) {
      if (!req.prevStates.empty()) {
         invokeJsCallBack(jsCallback, QJSValueList() << QJSValue(false) << tr("Transaction already signed"));
         return;
      }
      if (req.walletIds.empty()) {
         invokeJsCallBack(jsCallback, QJSValueList() << QJSValue(false) << tr("Missing wallet ID[s] in request"));
         return;
      }
      const auto rootWallet = walletsMgr_->getHDRootForLeaf(req.walletIds.front());
      if (!rootWallet) {
         invokeJsCallBack(jsCallback, QJSValueList() << QJSValue(false) << tr("Failed to find root wallet for ID %1")
            .arg(QString::fromStdString(req.walletIds.front())));
         return;
      }

      parsedReqsForWallets->operator[](rootWallet->walletId()).push_back(req);
   }

   // sign reqs by wallets
   const auto &requestCbs = std::make_shared<std::vector<std::function<void()>>>();

   for (const auto &req : *parsedReqsForWallets) {
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
                     invokeJsCallBack(jsCallback, QJSValueList()
                        << QJSValue(false)
                        << tr("Failed to sign request, error code: %1, file: %2")
                           .arg(static_cast<int>(result))
                           .arg(fileName));
                     return;
                  }
                  QFileInfo fi(fileName);
                  QString outputFN = fi.path() + QLatin1String("/") + fi.baseName() + QLatin1String("_signed.bin");

                  bs::error::ErrorCode exportResult = bs::core::wallet::ExportSignedTxToFile(signedTX, outputFN, reqs[0].comment);

                  if (exportResult != bs::error::ErrorCode::NoError) {
                     invokeJsCallBack(jsCallback, QJSValueList()
                        << QJSValue(false)
                        << tr("%1\n%2").arg(bs::error::ErrorCodeToString(exportResult)).arg(outputFN));
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
         bs::wallet::TXInfo *txInfo = new bs::wallet::TXInfo(reqs[0], walletsMgr_, logger_);
         QQmlEngine::setObjectOwnership(txInfo, QQmlEngine::JavaScriptOwnership);

         bs::sync::PasswordDialogData *dialogData = new bs::sync::PasswordDialogData();
         QQmlEngine::setObjectOwnership(dialogData, QQmlEngine::JavaScriptOwnership);
         dialogData->setValue(bs::sync::PasswordDialogData::Title, tr("Sign Offline TX"));

         bs::hd::WalletInfo *walletInfo = adapter_->qmlFactory()->createWalletInfo(walletId);

         adapter_->qmlBridge()->invokeQmlMethod(QmlBridge::CreateTxSignDialog, cb
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
   if (!walletsMgr_) {
      return false;
   }

   return walletsMgr_->walletNameExists(name.toStdString());
}

QString WalletsProxy::generateNextWalletName() const
{
   if (!walletsMgr_) {
      return {};
   }

   QString newWalletName;
   size_t nextNumber = walletsMgr_->hdWallets().size() + 1;
   do {
      newWalletName = tr("Wallet #%1").arg(nextNumber);
      nextNumber++;
   } while (walletNameExists(newWalletName));
   return newWalletName;
}

bool WalletsProxy::isWatchingOnlyWallet(const QString &walletId) const
{
   if (!walletsMgr_) {
      return false;
   }

   return walletsMgr_->isWatchingOnly(walletId.toStdString());
}

QString WalletsProxy::defaultBackupLocation() const
{
   return QString::fromLatin1("file://") +
      QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
}

void WalletsProxy::createWallet(bool isPrimary, bs::wallet::QSeed *seed, bs::hd::WalletInfo *walletInfo
   , bs::wallet::QPasswordData *passwordData, const QJSValue &jsCallback)
{
   auto cb = [this, jsCallback] (bs::error::ErrorCode errorCode) {
      QMetaObject::invokeMethod(this, [this, errorCode, jsCallback] {
         QJSValueList args;
         args << QJSValue(errorCode == bs::error::ErrorCode::NoError)
              << bs::error::ErrorCodeToString(errorCode);
         invokeJsCallBack(jsCallback, args);
      });
   };

   if (!walletsMgr_) {
      QMetaObject::invokeMethod(this, [this, jsCallback] {
         invokeJsCallBack(jsCallback, QJSValueList() << QJSValue(false) << QJSValue(tr("Internal error")));
      });
      return;
   }
   if (seed->networkType() == bs::wallet::QSeed::Invalid) {
      QMetaObject::invokeMethod(this, [this, jsCallback] {
         invokeJsCallBack(jsCallback, QJSValueList() << QJSValue(false) << QJSValue(tr("Failed to create wallet with invalid seed")));
      });
      return;
   }

   adapter_->createWallet(walletInfo->name().toStdString(), walletInfo->desc().toStdString()
      , *seed, isPrimary, *passwordData, cb);
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
      auto result = std::make_shared<bs::sync::hd::Wallet>(wo, signContainer().get(), logger_);
      result->setWCT(walletsMgr_.get());
      for (const auto &groupEntry : wo.groups) {
         auto group = result->createGroup(static_cast<bs::hd::CoinType>(groupEntry.type), false);
         for (const auto &leafEntry : groupEntry.leaves) {
            group->createLeaf(leafEntry.path, leafEntry.id);
         }
      }
      return result;
   } catch (const std::exception &e) {
      logger_->error("[WalletsProxy] WO-wallet creation failed: {}", e.what());
   }
   return nullptr;
}

std::shared_ptr<SignAdapterContainer> WalletsProxy::signContainer() const
{
   return signContainer_;
}

std::function<void (bs::error::ErrorCode result)> WalletsProxy::createChangePwdResultCb(const QString &walletId, const QJSValue &jsCallback)
{
   return [this, walletId, jsCallback](bs::error::ErrorCode result) {
        QMetaObject::invokeMethod(this, [this, jsCallback, result] {
           invokeJsCallBack(jsCallback, QJSValueList()
              << QJSValue(result == bs::error::ErrorCode::NoError)
              << QJSValue(bs::error::ErrorCodeToString(result)));
        });

        if (result == bs::error::ErrorCode::NoError) {
           onWalletsChanged();
        }
     };
}

void WalletsProxy::importWoWallet(const QString &walletPath, const QJSValue &jsCallback)
{
   auto cb = [this, jsCallback](const bs::sync::WatchingOnlyWallet &wo) {
      QMetaObject::invokeMethod(this, [this, wo, jsCallback] {
         logger_->debug("imported WO wallet with id {}", wo.id);
         walletsMgr_->adoptNewWallet(getWoSyncWallet(wo));
         QJSValueList args;
         args << QJSValue(wo.id.empty() ? false : true)
            << QString::fromStdString(wo.id)
            << QString::fromStdString(wo.name)
            << QString::fromStdString(wo.description);
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
   const auto content = BinaryData::fromString(f.readAll().toStdString());
   f.close();

   QFileInfo fi(walletPath);

   adapter_->importWoWallet(fi.fileName().toStdString(), content, cb);
}

void WalletsProxy::importHwWallet(HwWalletWrapper walletInfo, const QJSValue &jsCallback)
{
   auto cb = [this, jsCallback](const bs::sync::WatchingOnlyWallet &wo) {
      QMetaObject::invokeMethod(this, [this, wo, jsCallback] {
         logger_->debug("imported WO wallet with id {}", wo.id);
         QJSValueList args;
         args << QJSValue(wo.id.empty() ? false : true)
            << QString::fromStdString(wo.id)
            << QString::fromStdString(wo.name)
            << QString::fromStdString(wo.description);
         invokeJsCallBack(jsCallback, args);
      });
   };

   adapter_->importHwWallet(walletInfo.info_, cb);
}

QStringList WalletsProxy::walletNames() const
{
   if (!walletsMgr_) {
      return {};
   }

   QStringList result;
   for (const auto &wallet : walletsMgr_->hdWallets()) {
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
   if (!walletsMgr_) {
      return 0;
   }

   size_t i = 0;
   for (const auto &wallet : walletsMgr_->hdWallets()) {
      if (wallet->walletId() == walletId.toStdString()) {
         return i;
      }
      i++;
   }
   return 0;
}

QString WalletsProxy::walletIdForIndex(int index) const
{
   if (!walletsMgr_) {
      return {};
   }

   const auto &hdWallets = walletsMgr_->hdWallets();
   if ((index < 0) || (index >= hdWallets.size())) {
      return {};
   }
   return QString::fromStdString(hdWallets[index]->walletId());
}

void WalletsProxy::sendControlPassword(bs::wallet::QPasswordData *password)
{
   if (password) {
      adapter_->sendControlPassword(*password);
   }
}

void WalletsProxy::changeControlPassword(bs::wallet::QPasswordData *oldPassword, bs::wallet::QPasswordData *newPassword
   , const QJSValue &jsCallback)
{
   const auto cb = [this, jsCallback](bs::error::ErrorCode result) {
      QMetaObject::invokeMethod(this, [this, jsCallback, result] {
         invokeJsCallBack(jsCallback, QJSValueList()
            << QJSValue(result == bs::error::ErrorCode::NoError)
            << QJSValue(bs::error::ErrorCodeToString(result)));
      });

      if (result == bs::error::ErrorCode::NoError) {
         onWalletsChanged();
      }
   };

   if (oldPassword) {
      const bs::wallet::QPasswordData &newPassDataRef = newPassword ? *newPassword : bs::wallet::QPasswordData();
      adapter_->changeControlPassword(*oldPassword, newPassDataRef, cb);
   }
}
