#ifndef __AUTH_PROXY_H__
#define __AUTH_PROXY_H__

#include "AutheIDClient.h"
#include "EncryptionUtils.h"
#include "MetaData.h"
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
   AuthObject(QObject *parent = nullptr)
      : QObject(parent) {}

signals:
   void statusChanged();
   void error(QString errMsg) const;

protected:
   QString status() const { return status_; }
   void setStatus(const QString &);

private:
   QString status_;
};

class AuthSignWalletObject : public AuthObject
{
   Q_OBJECT

public:
   AuthSignWalletObject(QObject *parent = nullptr) {}

   AuthSignWalletObject(const std::shared_ptr<spdlog::logger> &
                        , QObject *parent = nullptr);

   // used for wallet creation and signing
   bool signWallet(AutheIDClient::RequestType requestType, bs::hd::WalletInfo *walletInfo);

   // used for device removing
   void removeDevice(int index, bs::hd::WalletInfo *walletInfo);

   Q_INVOKABLE void cancel();

signals:
   void succeeded(const QString &encKey, const SecureBinaryData &password) const;
   void failed(const QString &text) const;

private:
   std::shared_ptr<AutheIDClient> autheIDClient_;
};


#endif // __AUTH_PROXY_H__
