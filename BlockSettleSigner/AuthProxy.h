#ifndef __AUTH_PROXY_H__
#define __AUTH_PROXY_H__

#include "AutheIDClient.h"

#include <memory>
#include <QObject>

class ApplicationSettings;
class ConnectionManager;

namespace spdlog {
   class logger;
}

namespace bs {
namespace hd {
    class WalletInfo;
}
}


class AuthSignWalletObject : public QObject
{
   Q_OBJECT
   Q_PROPERTY(QString status READ status NOTIFY statusChanged)
public:
   AuthSignWalletObject(const std::shared_ptr<spdlog::logger> &
                        , const std::shared_ptr<ApplicationSettings> &
                        , const std::shared_ptr<ConnectionManager> &
                        , QObject *parent = nullptr);

   AuthSignWalletObject(QObject *parent = nullptr) : QObject(parent) {}
   AuthSignWalletObject(const AuthSignWalletObject &other);

   void connectToServer();

   // used for wallet creation and signing
   void signWallet(AutheIDClient::RequestType requestType, bs::hd::WalletInfo *walletInfo);

   // used for device removing
   void removeDevice(int index, bs::hd::WalletInfo *walletInfo);

   Q_INVOKABLE void cancel();

signals:
   void statusChanged();

   void succeeded(const QString &encKey, const SecureBinaryData &password) const;
   void failed(const QString &text) const;
   void userCancelled() const;


public:
   QString status() const { return status_; }
   void setStatus(const QString &status);
   std::shared_ptr<spdlog::logger> logger_;

private:
   std::shared_ptr<AutheIDClient> autheIDClient_;
   std::shared_ptr<ApplicationSettings> settings_;
   std::shared_ptr<ConnectionManager> connectionManager_;

   QString status_;
};


#endif // __AUTH_PROXY_H__
