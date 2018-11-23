#include <QFile>
#include <QVariant>
#include <QBuffer>
#include <QByteArray>
#include <QPixmap>

#include <spdlog/spdlog.h>

#include "AuthProxy.h"
#include "MobileClient.h"
#include "ApplicationSettings.h"

AuthProxy::AuthProxy(const std::shared_ptr<spdlog::logger> &logger, QObject *parent)
   : QObject(parent), logger_(logger)
{}

AuthSignWalletObject *AuthProxy::signWallet(const QString &userId, const QString &title, const QString &walletId)
{
   logger_->debug("[AuthProxy] signing {} for {}: {}", walletId.toStdString(), userId.toStdString(), title.toStdString());
   return new AuthSignWalletObject(logger_, userId, title, walletId, this);
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


AuthSignWalletObject::AuthSignWalletObject(const std::shared_ptr<spdlog::logger> &logger
   , const QString &userId, const QString &title, const QString &walletId, QObject *parent)
   : AuthObject(parent)
{
   ApplicationSettings settings;
   auto authKeys = settings.GetAuthKeys();
   mobileClient_ = (new MobileClient(spdlog::get(""), authKeys, this));

   //connect(mobileClient_, &MobileClient::succeeded, this, &WalletKeyWidget::onAuthSucceeded);
   //connect(mobileClient_, &MobileClient::failed, this, &WalletKeyWidget::onAuthFailed);

   //std::string serverPubKey = appSettings->get<std::string>(ApplicationSettings::authServerPubKey);
   //std::string serverHost = appSettings->get<std::string>(ApplicationSettings::authServerHost);
   //std::string serverPort = appSettings->get<std::string>(ApplicationSettings::authServerPort);

   //mobileClient_->init(serverPubKey, serverHost, serverPort);

   //mobileClient_->start(requestType_, ui_->comboBoxAuthId->currentText().toStdString()
   //   , walletId_, knownDeviceIds_);

/*
   connect(&freja_, &FrejaSignWallet::succeeded, [this](SecureBinaryData password) {
      emit success(QString::fromStdString(password.toHexStr()));
   });
   connect(&freja_, &FrejaSignWallet::failed, [this](const QString &text) { emit error(text); });
   connect(&freja_, &FrejaSignWallet::statusUpdated, [this](const QString &status) { setStatus(status); });

   freja_.start(userId, title, walletId.toStdString());
*/
}
