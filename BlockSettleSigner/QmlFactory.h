#ifndef QMLFACTORY_H
#define QMLFACTORY_H

#include <QObject>
#include <QQmlEngine>
#include <QQuickWindow>

#include "ApplicationSettings.h"
#include "QWalletInfo.h"
#include "QSeed.h"
#include "QPasswordData.h"
#include "AuthProxy.h"
#include "ConnectionManager.h"

#include "BSErrorCode.h"

class SignerAdapter;
namespace bs {
   namespace sync {
      class WalletsManager;
   }
}

class QmlFactory : public QObject
{
   Q_OBJECT
   Q_PROPERTY(QString headlessPubKey READ headlessPubKey WRITE setHeadlessPubKey NOTIFY headlessPubKeyChanged)

public:
   QmlFactory(const std::shared_ptr<ApplicationSettings> &settings
      , const std::shared_ptr<ConnectionManager> &connectionManager
      , const std::shared_ptr<spdlog::logger> &logger
      , QObject *parent = nullptr);

   void setWalletsManager(const std::shared_ptr<bs::sync::WalletsManager> &);

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
   Q_INVOKABLE bs::hd::WalletInfo *createWalletInfo() const;
   Q_INVOKABLE bs::hd::WalletInfo *createWalletInfo(const QString &walletId) const;
   Q_INVOKABLE bs::hd::WalletInfo *createWalletInfo(const std::string &walletId) const
   {
      return createWalletInfo(QString::fromStdString(walletId));
   }
   Q_INVOKABLE bs::hd::WalletInfo *createWalletInfo(int index) const;
   Q_INVOKABLE bs::hd::WalletInfo *createWalletInfoFromDigitalBackup(const QString &filename) const;

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
   QString headlessPubKey() const;

   // service functions
   Q_INVOKABLE void setClipboard(const QString &text) const;
   Q_INVOKABLE QString getClipboard() const;
   Q_INVOKABLE QRect frameSize(QObject *window) const;
   Q_INVOKABLE int titleBarHeight();
   Q_INVOKABLE void installEventFilterToObj(QObject *object);
   Q_INVOKABLE void applyWindowFix(QQuickWindow *mw);
   bool eventFilter(QObject *object, QEvent *event) override;

   Q_INVOKABLE int errorCodeNoError()    {return static_cast<int>(bs::error::ErrorCode::NoError); }
   Q_INVOKABLE int errorCodeTxCanceled() {return static_cast<int>(bs::error::ErrorCode::TxCanceled); }

signals:
   void closeEventReceived();
   void headlessPubKeyChanged();
   void showTrayNotify(const QString &title, const QString &msg);

public slots:
   void setHeadlessPubKey(const QString &headlessPubKey);

private:
   std::shared_ptr<bs::sync::WalletsManager> walletsMgr_;
   std::shared_ptr<ApplicationSettings> settings_;
   std::shared_ptr<ConnectionManager> connectionManager_;
   std::shared_ptr<spdlog::logger> logger_;

   QString headlessPubKey_;
};


#endif // QMLFACTORY_H
