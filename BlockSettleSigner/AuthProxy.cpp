/*

***********************************************************************************
* Copyright (C) 2018 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "AuthProxy.h"

#include <botan/hex.h>
#include <spdlog/spdlog.h>

#include <QBuffer>
#include <QByteArray>
#include <QFile>
#include <QJSValue>
#include <QPixmap>
#include <QVariant>

#include "ApplicationSettings.h"
#include "ConnectionManager.h"
#include "QWalletInfo.h"

AuthSignWalletObject::AuthSignWalletObject(const std::shared_ptr<spdlog::logger> &logger
   , const std::shared_ptr<ApplicationSettings> &settings, const std::shared_ptr<ConnectionManager> &connectionManager)
   : logger_(logger)
   , settings_(settings)
   , connectionManager_(connectionManager)
{
}

void AuthSignWalletObject::cancel()
{
   if (autheIDClient_) {
      autheIDClient_->cancel();
   }
}

void AuthSignWalletObject::setStatus(const QString &status)
{
   status_ = tr("Auth status: %1").arg(status);
   emit statusChanged();
}

AuthSignWalletObject::AuthSignWalletObject(const AuthSignWalletObject &other)
   : QObject(nullptr)
{
   settings_ = other.settings_;
   connectionManager_ = other.connectionManager_;
}

void AuthSignWalletObject::connectToServer()
{
   auto authKeys = settings_->GetAuthKeys();
   autheIDClient_ = std::make_shared<AutheIDClient>(logger_, connectionManager_->GetNAM(), settings_->GetAuthKeys(), settings_->autheidEnv(), this);

   connect(autheIDClient_.get(), &AutheIDClient::succeeded, this, [this](const std::string &encKey, const SecureBinaryData &password){
      emit succeeded(QString::fromStdString(encKey), password);
   });
   connect(autheIDClient_.get(), &AutheIDClient::failed, this, [this](AutheIDClient::ErrorType authError){
      if (authError == AutheIDClient::Timeout) {
         emit cancelledByTimeout();
         return;
      }
      emit failed(AutheIDClient::errorString(authError));
   });
   connect(autheIDClient_.get(), &AutheIDClient::userCancelled, this, &AuthSignWalletObject::userCancelled);
}

void AuthSignWalletObject::signWallet(AutheIDClient::RequestType requestType, bs::hd::WalletInfo *walletInfo,
   const QString& authEidMessage, int expiration, int timestamp)
{
   std::vector<std::string> knownDeviceIds;
   std::vector<std::string> userIds;
   // send auth to all devices stored in encKeys
   for (const QString& encKey: walletInfo->encKeys()) {
      auto deviceInfo = AutheIDClient::getDeviceInfo(SecureBinaryData::fromString(encKey.toStdString()).toBinStr());

      // deviceInfo is empty for ActivateWallet and is not empty for another requests
      if (!deviceInfo.deviceId.empty()) {
         knownDeviceIds.push_back(deviceInfo.deviceId);
      }

      if (!deviceInfo.userId.empty()) {
         userIds.push_back(deviceInfo.userId);
      }
   }

   try {
      if (userIds.empty()) {
         throw std::runtime_error("Auth eID email not found when signing");
      }
      autheIDClient_->getDeviceKey(requestType, userIds[0]
         , walletInfo->rootId().toStdString(), authEidMessage, knownDeviceIds, "", expiration, timestamp);
   }
   catch (const std::exception &e) {
      logger_->error("AuthEidClient failed to sign wallet: {}", e.what());
      QMetaObject::invokeMethod(this, [this, e](){
         emit failed(QString::fromStdString(e.what()));
      },
      Qt::QueuedConnection);
   }
}

void AuthSignWalletObject::activateWallet(const QString &walletId, const QString& authEidMessage, QJSValue &jsCallback)
{
   // Same flow as adding device except old email checks
   addDevice(walletId, authEidMessage, jsCallback, {});
}

void AuthSignWalletObject::addDevice(const QString &walletId, const QString &authEidMessage, QJSValue &jsCallback, const QString &oldEmail)
{
   auto qrSecret = Botan::hex_encode(autheid::generatePrivateKey());
   autheIDClient_->getDeviceKey(AutheIDClient::RequestType::ActivateWallet, ""
      , walletId.toStdString(), authEidMessage, {}, qrSecret, AutheIDClient::kDefaultExpiration, 0, oldEmail.toStdString());
   connect(autheIDClient_.get(), &AutheIDClient::requestIdReceived, this, [this, jsCallback, qrSecret](const std::string &requestId) mutable {
      auto pubKey = Botan::hex_encode(settings_->GetAuthKeys().second);
      auto url = fmt::format("https://autheid.com/app/requests/?request_id={}&ra_pub_key={}&qr_secret={}", requestId, pubKey, qrSecret);
      jsCallback.call({QJSValue(QString::fromStdString(url))});
   });
}

void AuthSignWalletObject::removeDevice(int index, bs::hd::WalletInfo *walletInfo, const QString &authEidMessage)
{
   // used only for removing existing devices
   // index is device index which should be removed

   if (walletInfo->encKeys().size() == 1) {
      emit failed(tr("Can't remove last device"));
      return;
   }

   if (index >= walletInfo->encKeys().size() || index < 0) {
      emit failed(tr("Incorrect index to delete"));
      return;
   }

   std::vector<std::string> knownDeviceIds;
   std::vector<std::string> userIds;
   // send auth to all devices stored in encKeys except device to be removed
   for (int i = 0; i < walletInfo->encKeys().size(); ++i) {
      if (index == i) continue;

      auto deviceInfo = AutheIDClient::getDeviceInfo(SecureBinaryData::fromString(walletInfo->encKeys().at(i).toStdString()).toBinStr());

      if (!deviceInfo.deviceId.empty()) {
         knownDeviceIds.push_back(deviceInfo.deviceId);
      }

      if (!deviceInfo.userId.empty()) {
         userIds.push_back(deviceInfo.userId);
      }
   }

   try {
      if (userIds.empty()) {
         throw std::runtime_error("Auth eID email not found at removal");
      }
      // currently we supports only single account for whole wallet, thus email stored in userIds[0]
      autheIDClient_->getDeviceKey(AutheIDClient::DeactivateWalletDevice, userIds[0]
         , walletInfo->rootId().toStdString(), authEidMessage, knownDeviceIds);
   }
   catch (const std::exception &e) {
      logger_->error("AuthEidClient failed to sign wallet: {}", e.what());
      QMetaObject::invokeMethod(this, [this, e](){
         emit failed(QString::fromStdString(e.what()));
      },
      Qt::QueuedConnection);
   }
}

