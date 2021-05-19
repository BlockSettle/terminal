/*

***********************************************************************************
* Copyright (C) 2018 - 2021, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMetaMethod>
#include <QPixmap>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QVariant>

#include <spdlog/spdlog.h>
#include "ArmoryBackups.h"
#include "CoreHDWallet.h"
#include "PaperBackupWriter.h"
#include "SignerAdapter.h"
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

void WalletsProxy::exportWatchingOnly(const QString &walletId, const QString &filePath, const QJSValue &jsCallback)
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

   SPDLOG_LOGGER_DEBUG(logger_, "copy WO from WO wallet to '{}'", filePath.toStdString());

   bool isHw = walletsMgr_->getHDWalletById(walletId.toStdString())->isHardwareWallet();
   adapter_->exportWoWallet(walletId.toStdString(), [this, walletId, successCallback, failCallback, filePath, isHw](const BinaryData &content) {
      if (content.empty()) {
         failCallback("can't read WO file");
         return;
      }

      {  QFile f(filePath);
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
      }

      if (isHw) {
         try {
            bs::core::hd::Wallet wallet(filePath.toStdString(), adapter_->netType());
            wallet.convertHardwareToWo();
         } catch (const std::exception &e) {
            SPDLOG_LOGGER_ERROR(logger_, "converting HW to WO wallet failed: {}", e.what());
            failCallback("unexpected error");
            return;
         }

         QFileInfo info(filePath);
         QString lockFilePath = info.path() + QDir::separator() + info.baseName() + QStringLiteral(".lmdb-lock");
         QFile::remove(lockFilePath);
      }

      successCallback();
   });
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
         const auto& encoded = ArmoryBackups::BackupEasy16::encode(chainCode
            , ArmoryBackups::BackupType::BIP32_Seed_Structured);
         if (encoded.size() != 2) {
            throw std::runtime_error("failed to encode easy16");
         }
         seedData.part1 = encoded.at(0);
         seedData.part2 = encoded.at(1);
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

   auto parsedReqs = bs::core::wallet::ParseOfflineTXFile(data);
   if (parsedReqs.empty()) {
      invokeJsCallBack(jsCallback, QJSValueList() << QJSValue(false) << tr("File %1 contains no TX sign requests").arg(fileName));
      return;
   }

   adapter_->verifyOfflineTxRequest(BinaryData::fromString(data)
      , [this, fileName, jsCallback, parsedReqs = std::move(parsedReqs), data](bs::error::ErrorCode errorCode) {
      if (errorCode != bs::error::ErrorCode::NoError) {
         invokeJsCallBack(jsCallback, QJSValueList()
            << QJSValue(false)
            << tr("Sign request verification failed: %1").arg(bs::error::ErrorCodeToString(errorCode)));
         return;
      }

      signOfflineTxProceed(fileName, parsedReqs, jsCallback);
   });
}

void WalletsProxy::signOfflineTxProceed(const QString &fileName, const std::vector<bs::core::wallet::TXSignRequest> &parsedReqs, const QJSValue &jsCallback)
{
   struct Requests
   {
      std::vector<bs::core::wallet::TXSignRequest> requests;
      bool isHw{};
   };

   // sort reqs by wallets
   const auto &parsedReqsForWallets = std::make_shared<std::unordered_map<std::string, Requests>>(); // <wallet_id, Requests>
   //const auto walletsMgr = adapter_->getWalletsManager();
   for (const auto &req : parsedReqs) {
      if (req.armorySigner_.isSigned()) {
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

      auto data = &parsedReqsForWallets->operator[](rootWallet->walletId());
      data->requests.push_back(req);
      data->isHw = rootWallet->isHardwareWallet();
   }

   // sign reqs by wallets
   const auto &requestCbs = std::make_shared<std::vector<std::function<void()>>>();

   for (const auto &req : *parsedReqsForWallets) {
      const auto &walletCb = [this, fileName, jsCallback, requestCbs, hdWalledId=req.first, reqs=req.second]() {

         const auto &cb = new bs::signer::QmlCallback<int, QString, bs::wallet::QPasswordData *>
               ([this, fileName, jsCallback, requestCbs, hdWalledId, reqs](int result, const QString &, bs::wallet::QPasswordData *passwordData){

            auto errorCode = static_cast<bs::error::ErrorCode>(result);
            if (errorCode == bs::error::ErrorCode::TxCancelled) {
               return;
            }
            else {
               const auto &cbSigned = [this, fileName, jsCallback, requestCbs, hdWalledId, reqs] (bs::error::ErrorCode result, const BinaryData &signedTX) {
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

                  bs::error::ErrorCode exportResult = bs::core::wallet::ExportSignedTxToFile(signedTX, outputFN
                     , reqs.requests[0].allowBroadcasts, reqs.requests[0].comment);

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

               bool isHwSigned = false;
               if (reqs.isHw) {
                  auto rootWallet = walletsMgr_->getHDWalletById(hdWalledId);

                  if (rootWallet && rootWallet->isHardwareWallet()) {
                     bs::wallet::HardwareEncKey hwEncKey(rootWallet->encryptionKeys()[0]);
                     // Trezor return already composed tx, nothing to do with it
                     isHwSigned = (hwEncKey.deviceType() == bs::wallet::HardwareEncKey::WalletType::Trezor);
                  }
               } 

               if (isHwSigned) {
                  // Signed request is stored in binaryPassword field for HW wallets
                  cbSigned(bs::error::ErrorCode::NoError, passwordData->binaryPassword());
               }
               else {
                  adapter_->signOfflineTxRequest(reqs.requests[0], passwordData->binaryPassword(), cbSigned);
               }
            }
         });


         // TODO: send to qml list of txInfo
         bs::wallet::TXInfo *txInfo = new bs::wallet::TXInfo(reqs.requests[0], walletsMgr_, logger_);
         QQmlEngine::setObjectOwnership(txInfo, QQmlEngine::JavaScriptOwnership);

         bs::sync::PasswordDialogData *dialogData = new bs::sync::PasswordDialogData();
         QQmlEngine::setObjectOwnership(dialogData, QQmlEngine::JavaScriptOwnership);
         dialogData->setValue(bs::sync::PasswordDialogData::Title, tr("Sign Offline TX"));

         if (reqs.isHw) {
            // Pass sign request as it is, HW wallet will handle it
            auto reqData = coreTxRequestToPb(reqs.requests[0]).SerializeAsString();
            dialogData->setValue(bs::sync::PasswordDialogData::TxRequest, QByteArray::fromStdString(reqData));
         }

         bs::hd::WalletInfo *walletInfo = adapter_->qmlFactory()->createWalletInfo(hdWalledId);

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

void WalletsProxy::createWallet(bool isPrimary, bool createLegacyLeaf, bs::wallet::QSeed *seed, bs::hd::WalletInfo *walletInfo
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
      , *seed, isPrimary, createLegacyLeaf, *passwordData, cb);
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
#if 0 //FIXME: unknown sync::hd::Wallet constructor
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
#endif   //0
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

   try {
      const auto wallet = std::make_shared<bs::core::hd::Wallet>(fi.fileName().toStdString()
         , adapter_->netType(), fi.path().toStdString(), SecureBinaryData(), logger_);
      if (wallet->networkType() != adapter_->netType()) {
         SPDLOG_LOGGER_ERROR(logger_, "invalid net type in WO file: {}, expected: {}"
            , static_cast<int>(wallet->networkType()), static_cast<int>(adapter_->netType()));
         if (adapter_->netType() == NetworkType::MainNet) {
            errWallet.description = "Can not import testnet WO wallet";
         } else {
            errWallet.description = "Can not import mainnet WO wallet";
         }
         cb(errWallet);
         return;
      }
   } catch (const std::exception &e) {
      SPDLOG_LOGGER_ERROR(logger_, "loading WO wallet failed: {}", e.what());
      errWallet.description = fmt::format("Loading WO wallet failed: {}", e.what());
      cb(errWallet);
      return;
   }

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

   if (!walletInfo.isFirmwareSupported_) {
      bs::sync::WatchingOnlyWallet result;
      result.description = walletInfo.firmwareSupportedMsg_;
      cb(result);
      return;
   }

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

QStringList WalletsProxy::priWalletNames() const
{
   if (!walletsMgr_) {
      return {};
   }

   QStringList result;
   for (const auto &wallet : walletsMgr_->hdWallets()) {
      if (wallet->isPrimary()) {
         result.push_back(QString::fromStdString(wallet->name()));
      }
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

QString WalletsProxy::walletIdForName(const QString &name) const
{
   if (!walletsMgr_) {
      return {};
   }
   for (const auto &wallet : walletsMgr_->hdWallets()) {
      if (wallet->name() == name.toStdString()) {
         return QString::fromStdString(wallet->walletId());
      }
   }
   return {};
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

QString WalletsProxy::pixmapToDataUrl(const QPixmap &pixmap) const
{
   QByteArray array;
   QBuffer buffer(&array);
   buffer.open(QIODevice::WriteOnly);
   pixmap.save(&buffer, "PNG");
   QString image(QStringLiteral("data:image/png;base64,") + QString::fromLatin1(array.toBase64().data()));
   return image;
}

QPixmap WalletsProxy::getQRCode(const QString &data, int size) const
{
   return UiUtils::getQRCode(data, size);
}
