#ifndef QMLFACTORY_H
#define QMLFACTORY_H

#include <QObject>
#include <QQmlEngine>

#include "QWalletInfo.h"
#include "QSeed.h"
#include "QPasswordData.h"
#include "WalletsManager.h"
#include "AuthProxy.h"

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

   Q_INVOKABLE bs::wallet::QPasswordData *createPasswordData() {
      auto pd = new bs::wallet::QPasswordData();
      QQmlEngine::setObjectOwnership(pd, QQmlEngine::JavaScriptOwnership);
      return pd;
   }

   // QSeed
   Q_INVOKABLE bs::wallet::QSeed *createSeed(bool isTestNet){
      auto seed = new bs::wallet::QSeed(isTestNet);
      QQmlEngine::setObjectOwnership(seed, QQmlEngine::JavaScriptOwnership);
      return seed;
   }

   Q_INVOKABLE bs::wallet::QSeed *createSeedFromPaperBackup(const QString &key, bs::wallet::QSeed::QNetworkType netType) {
      auto seed = new bs::wallet::QSeed(bs::wallet::QSeed::fromPaperKey(key, netType));
      QQmlEngine::setObjectOwnership(seed, QQmlEngine::JavaScriptOwnership);
      return seed;
   }

   Q_INVOKABLE bs::wallet::QSeed *createSeedFromPaperBackupT(const QString &key, bool isTestNet) {
      auto seed = new bs::wallet::QSeed(bs::wallet::QSeed::fromPaperKey(key
                                        , isTestNet ? bs::wallet::QSeed::QNetworkType::TestNet : bs::wallet::QSeed::QNetworkType::MainNet));
      QQmlEngine::setObjectOwnership(seed, QQmlEngine::JavaScriptOwnership);
      return seed;
   }

   Q_INVOKABLE bs::wallet::QSeed *createSeedFromDigitalBackup(const QString &filename, bs::wallet::QSeed::QNetworkType netType) {
      auto seed = new bs::wallet::QSeed(bs::wallet::QSeed::fromDigitalBackup(filename, netType));
      QQmlEngine::setObjectOwnership(seed, QQmlEngine::JavaScriptOwnership);
      return seed;
   }

   Q_INVOKABLE bs::wallet::QSeed *createSeedFromDigitalBackupT(const QString &filename, bool isTestNet) {
      auto seed = new bs::wallet::QSeed(bs::wallet::QSeed::fromDigitalBackup(filename
                                                                             , isTestNet ? bs::wallet::QSeed::QNetworkType::TestNet : bs::wallet::QSeed::QNetworkType::MainNet));
      QQmlEngine::setObjectOwnership(seed, QQmlEngine::JavaScriptOwnership);
      return seed;
   }

   // WalletInfo
   Q_INVOKABLE bs::hd::WalletInfo *createWalletInfo();
   Q_INVOKABLE bs::hd::WalletInfo *createWalletInfo(const QString &walletId);
   Q_INVOKABLE bs::hd::WalletInfo *createWalletInfo(const std::string &walletId) { return createWalletInfo(QString::fromStdString(walletId)); }
   Q_INVOKABLE bs::hd::WalletInfo *createWalletInfoFromDigitalBackup(const QString &filename);



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

   Q_INVOKABLE void setClipboard(const QString &text);
private:
   std::shared_ptr<WalletsManager> walletsMgr_;
   std::shared_ptr<spdlog::logger> logger_;
};


#endif // QMLFACTORY_H
