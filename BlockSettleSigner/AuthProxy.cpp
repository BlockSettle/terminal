﻿#include <QFile>
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

AuthSignWalletObject *AuthProxy::signWallet(MobileClient::RequestType requestType, const QString &userId,
                                            const QString &title, const QString &walletId,
                                            const QString &encKey)
{
   logger_->debug("[AuthProxy] signing {} for {}: {}", walletId.toStdString(), userId.toStdString(), title.toStdString());
   return new AuthSignWalletObject(requestType, logger_, userId, title, walletId, encKey, this);
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


AuthSignWalletObject::AuthSignWalletObject(MobileClient::RequestType requestType, const std::shared_ptr<spdlog::logger> &logger
   , const QString &userId, const QString &title, const QString &walletId, const QString &encKey, QObject *parent)
   : AuthObject(parent)
{
   ApplicationSettings settings;
   auto authKeys = settings.GetAuthKeys();
   mobileClient_ = (new MobileClient(logger, authKeys, this));

   connect(mobileClient_, &MobileClient::succeeded, this, [this](const std::string &encKey, const SecureBinaryData &password){
       emit succeeded(QString::fromStdString(encKey), password);
   });
   connect(mobileClient_, &MobileClient::failed, this, [this](const QString &text){
       emit failed(text);
   });
   std::string serverPubKey = settings.get<std::string>(ApplicationSettings::authServerPubKey);
   std::string serverHost = settings.get<std::string>(ApplicationSettings::authServerHost);
   std::string serverPort = settings.get<std::string>(ApplicationSettings::authServerPort);

   mobileClient_->init(serverPubKey, serverHost, serverPort);

   std::vector<std::string> knownDeviceIds_;
   auto deviceInfo = MobileClient::getDeviceInfo(SecureBinaryData(encKey.toStdString()).toBinStr());
   if (!deviceInfo.deviceId.empty()) {
       knownDeviceIds_.push_back(deviceInfo.deviceId);
   }

   mobileClient_->start(requestType, userId.toStdString()
      , walletId.toStdString(), knownDeviceIds_);
//   mobileClient_->start(MobileClient::ActivateWallet, userId.toStdString()
//      , walletId.toStdString(), knownDeviceIds_);

/*
   connect(&freja_, &FrejaSignWallet::succeeded, [this](SecureBinaryData password) {
      emit success(QString::fromStdString(password.toHexStr()));
   });
   connect(&freja_, &FrejaSignWallet::failed, [this](const QString &text) { emit error(text); });
   connect(&freja_, &FrejaSignWallet::statusUpdated, [this](const QString &status) { setStatus(status); });

   freja_.start(userId, title, walletId.toStdString());
*/
}
