#include <QFile>
#include <QVariant>
#include <QBuffer>
#include <QByteArray>
#include <QPixmap>

#include <spdlog/spdlog.h>

#include "AuthProxy.h"
#include "ApplicationSettings.h"

AuthProxy::AuthProxy(const std::shared_ptr<spdlog::logger> &logger, QObject *parent)
   : QObject(parent), logger_(logger)
{}

AuthSignWalletObject *AuthProxy::signWallet(AutheIDClient::RequestType requestType
                                            , const QString &userId
                                            , const QString &title
                                            , const QString &walletId
                                            , const QString &encKey)
{
   logger_->debug("[AuthProxy] signing {} for {}: {}", walletId.toStdString(), userId.toStdString(), title.toStdString());
   return new AuthSignWalletObject(requestType, logger_, userId, title, walletId, encKey, this);
}

AuthSignWalletObject *AuthProxy::signWallet(AutheIDClient::RequestType requestType
                                            , const QString &title
                                            , const QString &walletId
                                            , const QString &encKey)
{
    std::string userId = AutheIDClient::getDeviceInfo(encKey.toStdString()).userId;
    logger_->debug("[AuthProxy] signing {} for {}: {}", walletId.toStdString(), userId, title.toStdString());

    return new AuthSignWalletObject(requestType,
                                    logger_,
                                    QString::fromStdString(userId),
                                    title,
                                    walletId,
                                    encKey,
                                    this);
}

void AuthSignWalletObject::cancel()
{
//   freja_.stop(true);
}


void AuthObject::setStatus(const QString &status)
{
   status_ = tr("Auth status: %1").arg(status);
   emit statusChanged();
}


AuthSignWalletObject::AuthSignWalletObject(AutheIDClient::RequestType requestType, const std::shared_ptr<spdlog::logger> &logger
   , const QString &userId, const QString &title, const QString &walletId, const QString &encKey, QObject *parent)
   : AuthObject(parent)
{
   ApplicationSettings settings;
   auto authKeys = settings.GetAuthKeys();
   AutheIDClient_ = (new AutheIDClient(logger, authKeys, this));

   connect(AutheIDClient_, &AutheIDClient::succeeded, this, [this](const std::string &encKey, const SecureBinaryData &password){
       emit succeeded(QString::fromStdString(encKey), password);
   });
   connect(AutheIDClient_, &AutheIDClient::failed, this, [this](const QString &text){
       emit failed(text);
   });
   std::string serverPubKey = settings.get<std::string>(ApplicationSettings::authServerPubKey);
   std::string serverHost = settings.get<std::string>(ApplicationSettings::authServerHost);
   std::string serverPort = settings.get<std::string>(ApplicationSettings::authServerPort);

   AutheIDClient_->connect(serverPubKey, serverHost, serverPort);

   std::vector<std::string> knownDeviceIds;
   auto deviceInfo = AutheIDClient::getDeviceInfo(SecureBinaryData(encKey.toStdString()).toBinStr());

   // deviceInfo is empty for ActivateWallet and is not empty for another requests
   if (!deviceInfo.deviceId.empty()) {
       knownDeviceIds.push_back(deviceInfo.deviceId);
   }

   AutheIDClient_->start(requestType, userId.toStdString()
      , walletId.toStdString(), knownDeviceIds);

}
