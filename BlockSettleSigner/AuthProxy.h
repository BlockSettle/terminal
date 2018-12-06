#ifndef __AUTH_PROXY_H__
#define __AUTH_PROXY_H__

#include <memory>
#include <QObject>
#include "EncryptionUtils.h"
#include "MobileClient.h"


//class MobileClient;
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
   QString  status_;
};

class AuthSignWalletObject : public AuthObject
{
   Q_OBJECT

public:
   AuthSignWalletObject() : AuthObject(nullptr) {}
   AuthSignWalletObject(MobileClient::RequestType requestType, const std::shared_ptr<spdlog::logger> &, const QString &userId
      , const QString &title, const QString &walletId, const QString &encKey, QObject *parent = nullptr);

   Q_INVOKABLE void cancel();

signals:
   void succeeded(const QString &encKey, const SecureBinaryData &password) const;
   void failed(const QString &text) const;

private:
//   FrejaSignWallet   freja_;
   MobileClient *mobileClient_{};
};


class AuthProxy : public QObject
{
   Q_OBJECT
public:
   AuthProxy(const std::shared_ptr<spdlog::logger> &, QObject *parent = nullptr);

   Q_INVOKABLE AuthSignWalletObject *signWallet(MobileClient::RequestType requestType, const QString &userId, const QString &title
      , const QString &walletId, const QString &encKey);

private:
   std::shared_ptr<spdlog::logger>  logger_;
};

#endif // __AUTH_PROXY_H__
