/*

***********************************************************************************
* Copyright (C) 2018 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __AUTH_PROXY_H__
#define __AUTH_PROXY_H__

#include "AutheIDClient.h"

#include <memory>
#include <QObject>

class ApplicationSettings;
class ConnectionManager;
class QJSValue;

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
   AuthSignWalletObject(const std::shared_ptr<spdlog::logger> &, const std::shared_ptr<ApplicationSettings> &
      , const std::shared_ptr<ConnectionManager> &);

   AuthSignWalletObject() {}
   AuthSignWalletObject(const AuthSignWalletObject &other);

   void connectToServer();

   // used for wallet creation and signing
   void signWallet(AutheIDClient::RequestType requestType, bs::hd::WalletInfo *walletInfo
      , const QString& authEidMessage, int expiration = AutheIDClient::kDefaultExpiration, int timestamp = 0);
   void activateWallet(const QString &walletId, const QString &authEidMessage, QJSValue &jsCallback);
   void addDevice(const QString &walletId, const QString &authEidMessage, QJSValue &jsCallback, const QString &oldEmail);

   // used for device removing
   void removeDevice(int index, bs::hd::WalletInfo *walletInfo, const QString& authEidMessage);

   Q_INVOKABLE void cancel();

   Q_INVOKABLE int defaultSettlementExpiration() { return AutheIDClient::kDefaultSettlementExpiration; }
   Q_INVOKABLE int defaultExpiration() { return AutheIDClient::kDefaultExpiration; }

signals:
   void statusChanged();

   void succeeded(const QString &encKey, const SecureBinaryData &password) const;
   void failed(const QString &text) const;
   void userCancelled() const;
   void cancelledByTimeout() const;

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
