#include <QFile>
#include <QVariant>
#include <QPixmap>
#include <QStandardPaths>

#include "PaperBackupWriter.h"
#include "WalletBackupFile.h"
#include "WalletEncryption.h"
#include "QWalletInfo.h"
#include "QQmlEngine"
#include "WalletsManager.h"
#include "AuthProxy.h"

namespace bs {
   namespace wallet {
      QNetworkType toQNetworkType(NetworkType netType) { return static_cast<QNetworkType>(netType); }
      NetworkType fromQNetworkType(bs::wallet::QNetworkType netType) { return static_cast<NetworkType>(netType); }
   }
}

using namespace bs::hd;
using namespace bs::wallet;

WalletInfo::WalletInfo(const std::shared_ptr<WalletsManager> &walletsMgr
                       , const QString &walletId
                       , QObject *parent)
   : QObject(parent)
   , walletId_(walletId)
   , walletsManager_(walletsMgr)
{
   const auto &wallet = walletsMgr->GetWalletById(walletId.toStdString());
   if (wallet) {
      const auto &rootWallet = walletsMgr->GetHDRootForLeaf(wallet->GetWalletId());
      initFromWallet(wallet.get(), rootWallet->getWalletId());
      initEncKeys(rootWallet);
   }
   else {
      const auto &hdWallet = walletsMgr->GetHDWalletById(walletId.toStdString());
      if (!hdWallet) {
         // TODO: may be add isValid() function
         // throw std::runtime_error("failed to find wallet id " + walletId.toStdString());
      }
      else {
         initFromRootWallet(hdWallet);
         initEncKeys(hdWallet);
      }
   }

   if (walletsManager_) {
      connect(walletsManager_.get(), &WalletsManager::walletChanged, this, [&](){
         *this = WalletInfo(walletsManager_, walletId_, this->parent());
         emit walletChanged();
      });
   }
}

WalletInfo::WalletInfo(const WalletInfo &other)
   : walletId_(other.walletId_), rootId_(other.rootId_)
   , name_(other.name_), desc_(other.desc_)
   , encKeys_(other.encKeys_), encTypes_(other.encTypes_)
   , walletsManager_(other.walletsManager_)
{
   if (walletsManager_) {
      connect(walletsManager_.get(), &WalletsManager::walletChanged, this, [&](){
         emit walletChanged();
      });
   }
}

WalletInfo &bs::hd::WalletInfo::WalletInfo::operator =(const WalletInfo &other)
{
   walletId_ = other.walletId_;
   rootId_ = other.rootId_;
   name_ = other.name_;
   desc_ = other.desc_;
   encKeys_ = other.encKeys_;
   encTypes_ = other.encTypes_;
   walletsManager_ = other.walletsManager_;

   if (walletsManager_) {
      connect(walletsManager_.get(), &WalletsManager::walletChanged, this, [&](){
         emit walletChanged();
      });
   }

   return *this;
}

WalletInfo WalletInfo::fromDigitalBackup(const QString &filename)
{
   bs::hd::WalletInfo walletInfo;

   QFile file(filename);
   if (!file.exists()) return walletInfo;

   if (file.open(QIODevice::ReadOnly)) {
      QByteArray data = file.readAll();
      const auto wdb = WalletBackupFile::Deserialize(std::string(data.data(), data.size()));
      walletInfo.setName(QString::fromStdString(wdb.name));
      walletInfo.setDesc(QString::fromStdString(wdb.name));
   }
   return walletInfo;
}

void WalletInfo::initFromWallet(const bs::Wallet *wallet, const std::string &rootId)
{
   if (!wallet)
      return;

   walletId_ = QString::fromStdString(wallet->GetWalletId());
   rootId_ = QString::fromStdString(rootId);
   name_ = QString::fromStdString(wallet->GetWalletName());
   emit walletChanged();
}

void WalletInfo::initFromRootWallet(const std::shared_ptr<bs::hd::Wallet> &rootWallet)
{
   walletId_ = QString::fromStdString(rootWallet->getWalletId());
   name_ = QString::fromStdString(rootWallet->getName());
   rootId_ = QString::fromStdString(rootWallet->getWalletId());
   emit walletChanged();
}

void WalletInfo::initEncKeys(const std::shared_ptr<Wallet> &rootWallet)
{
   for (const SecureBinaryData &encKey : rootWallet->encryptionKeys()) {
      encKeys_.push_back(QString::fromStdString(encKey.toBinStr()));
   }

   for (const EncryptionType &encType : rootWallet->encryptionTypes()) {
      encTypes_.push_back(static_cast<bs::wallet::QEncryptionType>(encType));
   }
}

void WalletInfo::setDesc(const QString &desc)
{
   if (desc_ == desc)
      return;

   desc_ = desc;
   emit walletChanged();
}

void WalletInfo::setWalletId(const QString &walletId)
{
   if (walletId_ == walletId)
      return;

   walletId_ = walletId;
   emit walletChanged();
}

void WalletInfo::setRootId(const QString &rootId)
{
   if (rootId_ == rootId)
      return;

   rootId_ = rootId;
   emit walletChanged();
}

QEncryptionType WalletInfo::encType()
{
   return encTypes_.isEmpty() ? bs::wallet::QEncryptionType::Unencrypted : encTypes_.at(0);
}

QString WalletInfo::email()
{
   if (encKeys_.isEmpty())
      return QString();

   return QString::fromStdString(AutheIDClient::getDeviceInfo(encKeys_.at(0).toStdString()).userId);
}

void WalletInfo::setEncKeys(const QList<QString> &encKeys)
{
   encKeys_ = encKeys;
   emit walletChanged();
}

void WalletInfo::setEncTypes(const QList<QEncryptionType> &encTypes)
{
   encTypes_ = encTypes;
   emit walletChanged();
}

void WalletInfo::setName(const QString &name)
{
   if (name_ == name)
      return;

   name_ = name;
   emit walletChanged();
}

QSeed QSeed::fromPaperKey(const QString &key, QNetworkType netType)
{
   QSeed seed;
   try {
      const auto seedLines = key.split(QLatin1String("\n"), QString::SkipEmptyParts);
      if (seedLines.count() == 2) {
         EasyCoDec::Data easyData = { seedLines[0].toStdString(), seedLines[1].toStdString() };
         seed = bs::wallet::Seed::fromEasyCodeChecksum(easyData, bs::wallet::fromQNetworkType(netType));
      }
      else if (seedLines.count() == 4) {
         EasyCoDec::Data easyData = { seedLines[0].toStdString(), seedLines[1].toStdString() };
         EasyCoDec::Data edChainCode = { seedLines[2].toStdString(), seedLines[3].toStdString() };
         seed = bs::wallet::Seed::fromEasyCodeChecksum(easyData, edChainCode, bs::wallet::fromQNetworkType(netType));
      }
      else {
         seed.setSeed(key.toStdString());
      }
   }
   catch (const std::exception &e) {
      seed.lastError_ = tr("Failed to parse wallet key: %1").arg(QLatin1String(e.what()));
      return seed;
   }

   return seed;
}

QSeed QSeed::fromDigitalBackup(const QString &filename, QNetworkType netType)
{
   QSeed seed;

   QFile file(filename);
   if (!file.exists()) {
      seed.lastError_ = tr("Digital Backup file %1 doesn't exist").arg(filename);
      return seed;
   }
   if (file.open(QIODevice::ReadOnly)) {
      QByteArray data = file.readAll();
      const auto wdb = WalletBackupFile::Deserialize(std::string(data.data(), data.size()));
      if (wdb.id.empty()) {
         seed.lastError_ = tr("Digital Backup file %1 corrupted").arg(filename);
      }
      else {
         seed = bs::wallet::Seed::fromEasyCodeChecksum(wdb.seed, wdb.chainCode, bs::wallet::fromQNetworkType(netType));
      }
   }
   else {
      seed.lastError_ = tr("Failed to read Digital Backup file %1").arg(filename);
   }

   return seed;
}

QString QSeed::lastError() const
{
   return lastError_;
}

