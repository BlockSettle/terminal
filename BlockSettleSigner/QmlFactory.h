#ifndef QMLFACTORY_H
#define QMLFACTORY_H

#include <QObject>
#include "QWalletInfo.h"

class QmlFactory : public QObject
{
   Q_OBJECT
public:
   QmlFactory(std::shared_ptr<WalletsManager> walletsMgr
              , const std::shared_ptr<spdlog::logger> &logger
              , QObject *parent = nullptr)
      : walletsMgr_(walletsMgr)
      , logger_(logger)
      , QObject(parent) {}

   // QSeed
   Q_INVOKABLE bs::wallet::QPasswordData *createPasswordData()
   { return new bs::wallet::QPasswordData(); }

   Q_INVOKABLE bs::wallet::QSeed *createSeed(bool isTestNet)
   { return new bs::wallet::QSeed(isTestNet); }

   Q_INVOKABLE bs::wallet::QSeed *createSeedFromPaperBackup(const QString &key, bs::wallet::QNetworkType netType)
   { return new bs::wallet::QSeed(bs::wallet::QSeed::fromPaperKey(key, netType)); }

   Q_INVOKABLE bs::wallet::QSeed *createSeedFromPaperBackupT(const QString &key, bool isTestNet)
   { return new bs::wallet::QSeed(bs::wallet::QSeed::fromPaperKey(key
                                                                  , isTestNet ? bs::wallet::QNetworkType::TestNet : bs::wallet::QNetworkType::MainNet)); }

   Q_INVOKABLE bs::wallet::QSeed *createSeedFromDigitalBackup(const QString &filename, bs::wallet::QNetworkType netType)
   { return new bs::wallet::QSeed(bs::wallet::QSeed::fromDigitalBackup(filename, netType)); }

   Q_INVOKABLE bs::wallet::QSeed *createSeedFromDigitalBackupT(const QString &filename, bool isTestNet)
   { return new bs::wallet::QSeed(bs::wallet::QSeed::fromDigitalBackup(filename
                                                                       , isTestNet ? bs::wallet::QNetworkType::TestNet : bs::wallet::QNetworkType::MainNet)); }

   // WalletInfo
   Q_INVOKABLE bs::hd::WalletInfo *createWalletInfo()
   { return new bs::hd::WalletInfo(); }

   Q_INVOKABLE bs::hd::WalletInfo *createWalletInfo(const QString &walletId)
   { return new bs::hd::WalletInfo(walletsMgr_, walletId, this); }

   Q_INVOKABLE bs::hd::WalletInfo *createWalletInfoFromDigitalBackup(const QString &filename)
   { return new bs::hd::WalletInfo(bs::hd::WalletInfo::fromDigitalBackup(filename)); }



   // Auth
   // used for signing
   Q_INVOKABLE AuthSignWalletObject *createAutheIDSignObject(AutheIDClient::RequestType requestType
                                                             , bs::hd::WalletInfo *walletInfo);

   // used for add new eID device
   Q_INVOKABLE AuthSignWalletObject *createActivateEidObject(const QString &userId
                                                             , bs::hd::WalletInfo *walletInfo);

   // used for remove eID device
   // index: is encKeys index which should be deleted
   Q_INVOKABLE AuthSignWalletObject *createRemoveEidObject(int index
                                                             , bs::hd::WalletInfo *walletInfo);
private:
   std::shared_ptr<WalletsManager> walletsMgr_;
   std::shared_ptr<spdlog::logger> logger_;
};


#endif // QMLFACTORY_H
