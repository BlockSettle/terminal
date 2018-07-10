#ifndef __WALLETS_PROXY_H__
#define __WALLETS_PROXY_H__

#include <memory>
#include <QObject>
#include "MetaData.h"
#include "TXInfo.h"


namespace spdlog {
   class logger;
}
namespace bs {
   namespace hd {
      class Wallet;
   }
   class Wallet;
}
class SignerSettings;
class WalletsManager;

class WalletSeed : public QObject
{
   Q_OBJECT
   Q_PROPERTY(QString walletId READ walletId NOTIFY seedChanged)
   Q_PROPERTY(QString walletName READ walletName WRITE setWalletName NOTIFY seedChanged)
   Q_PROPERTY(QString walletDesc READ walletDesc WRITE setWalletDesc NOTIFY seedChanged)
   Q_PROPERTY(WalletInfo::EncryptionType encType READ encType WRITE setEncType NOTIFY seedChanged)
   Q_PROPERTY(QString encKey READ encKey WRITE setEncKey NOTIFY seedChanged)
   Q_PROPERTY(bool testNet READ isTestNet WRITE setTestNet NOTIFY seedChanged)
   Q_PROPERTY(bool mainNet READ isMainNet WRITE setMainNet NOTIFY seedChanged)

public:
   WalletSeed(QObject *parent = nullptr) : QObject(parent), seed_(NetworkType::MainNet) {}
   WalletSeed::WalletSeed(NetworkType netType, QObject *parent)
      : QObject(parent), seed_(netType) {}

   Q_INVOKABLE void setRandomKey();
   Q_INVOKABLE bool parsePaperKey(const QString &);
   Q_INVOKABLE bool parseDigitalBackupFile(const QString &);

   void setTestNet(bool) { seed_.setNetworkType(NetworkType::TestNet); emit seedChanged(); }
   bool isTestNet() const { return seed_.networkType() == NetworkType::TestNet; }
   void setMainNet(bool) { seed_.setNetworkType(NetworkType::MainNet); emit seedChanged(); }
   bool isMainNet() const { return seed_.networkType() == NetworkType::MainNet; }

   WalletInfo::EncryptionType encType() const { return static_cast<WalletInfo::EncryptionType>(seed_.encryptionType()); }
   void setEncType(WalletInfo::EncryptionType encType) { seed_.setEncryptionType(static_cast<bs::wallet::EncryptionType>(encType)); }
   QString encKey() const { return QString::fromStdString(seed_.encryptionKey().toBinStr()); }
   void setEncKey(const QString &key) { seed_.setEncryptionKey(key.toStdString()); }

   void setWalletName(const QString &name) { walletName_ = name; }
   void setWalletDesc(const QString &desc) { walletDesc_ = desc; }
   QString walletName() const { return walletName_; }
   QString walletDesc() const { return walletDesc_; }

   bs::wallet::Seed seed() const { return seed_; }
   QString walletId() const;

signals:
   void error(const QString &);
   void seedChanged() const;

private:
   bs::wallet::Seed  seed_;
   QString           walletName_;
   QString           walletDesc_;
};


class WalletsProxy : public QObject
{
   Q_OBJECT
   Q_PROPERTY(bool primaryWalletExists READ primaryWalletExists NOTIFY walletsChanged)
   Q_PROPERTY(bool loaded READ walletsLoaded NOTIFY walletsChanged)
   Q_PROPERTY(QStringList walletNames READ walletNames NOTIFY walletsChanged)

public:
   WalletsProxy(const std::shared_ptr<spdlog::logger> &, const std::shared_ptr<WalletsManager> &
      , const std::shared_ptr<SignerSettings> &);
   Q_INVOKABLE bool changePassword(const QString &walletId, const QString &oldPass, const QString &newPass
      , WalletInfo::EncryptionType, const QString &encKey);
   Q_INVOKABLE QString getWoWalletFile(const QString &walletId) const;
   Q_INVOKABLE bool exportWatchingOnly(const QString &walletId, QString path, const QString &password) const;
   Q_INVOKABLE bool backupPrivateKey(const QString &walletId, QString fileName, bool isPrintable
      , const QString &password) const;
   Q_INVOKABLE bool createWallet(bool isPrimary, const QString &password, WalletSeed *);
   Q_INVOKABLE bool importWallet(bool isPrimary, WalletSeed *, const QString &password);
   Q_INVOKABLE bool deleteWallet(const QString &walletId);

   Q_INVOKABLE QString getRootWalletId(const QString &walletId) const;
   Q_INVOKABLE QString getRootWalletName(const QString &walletId) const;

   Q_INVOKABLE int indexOfWalletId(const QString &walletId) const;
   Q_INVOKABLE QString walletIdForIndex(int) const;

   Q_INVOKABLE WalletSeed *createWalletSeed() const;

   bool walletsLoaded() const { return walletsLoaded_; }

signals:
   void walletError(const QString &walletId, const QString &errMsg) const;
   void walletsChanged();

private slots:
   void onWalletsChanged();

private:
   bool primaryWalletExists() const;
   std::shared_ptr<bs::hd::Wallet> getRootForId(const QString &walletId) const;
   QStringList walletNames() const;

private:
   std::shared_ptr<spdlog::logger>  logger_;
   std::shared_ptr<WalletsManager>  walletsMgr_;
   std::shared_ptr<SignerSettings>  params_;
   bool walletsLoaded_ = false;
};

#endif // __WALLETS_PROXY_H__
