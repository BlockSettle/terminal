#ifndef __AUTH_PROXY_H__
#define __AUTH_PROXY_H__

#include "AutheIDClient.h"
#include "ApplicationSettings.h"
#include "EncryptionUtils.h"
#include "QWalletInfo.h"
#include "WalletEncryption.h"

#include <memory>

#include <QObject>

class ApplicationSettings;

namespace spdlog {
   class logger;
}


class AuthObject : public QObject
{
   Q_OBJECT
   Q_PROPERTY(QString status READ status NOTIFY statusChanged)

public:
   AuthObject(std::shared_ptr<spdlog::logger> logger, QObject *parent = nullptr);

signals:
   void statusChanged();

protected:
   QString status() const { return status_; }
   void setStatus(const QString &);
   std::shared_ptr<spdlog::logger> logger_;

private:
   QString status_;
};

class AuthSignWalletObject : public AuthObject
{
   Q_OBJECT

public:
   AuthSignWalletObject(const std::shared_ptr<spdlog::logger> &
                        , const std::shared_ptr<ApplicationSettings> &
                        , const std::shared_ptr<ConnectionManager> &
                        , QObject *parent = nullptr);

   void connectToServer();

   // used for wallet creation and signing
   void signWallet(AutheIDClient::RequestType requestType, bs::hd::WalletInfo *walletInfo);

   // used for device removing
   void removeDevice(int index, bs::hd::WalletInfo *walletInfo);

   Q_INVOKABLE void cancel();

signals:
   void succeeded(const QString &encKey, const SecureBinaryData &password) const;
   void failed(const QString &text) const;
   void userCancelled() const;

private:
   std::shared_ptr<AutheIDClient> autheIDClient_;
   std::shared_ptr<ApplicationSettings> settings_;
   std::shared_ptr<ConnectionManager> connectionManager_;
};


#endif // __AUTH_PROXY_H__
