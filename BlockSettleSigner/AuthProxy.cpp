/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "AuthProxy.h"

#include <spdlog/spdlog.h>
#include <QFile>
#include <QVariant>
#include <QBuffer>
#include <QByteArray>
#include <QPixmap>

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
      emit failed(AutheIDClient::errorString(authError));
   });
   connect(autheIDClient_.get(), &AutheIDClient::userCancelled, this, [this](){
      emit userCancelled();
   });
}

void AuthSignWalletObject::signWallet(AutheIDClient::RequestType requestType, bs::hd::WalletInfo *walletInfo, int expiration, int timestamp)
{
   std::vector<std::string> knownDeviceIds;
   std::vector<std::string> userIds;
   // send auth to all devices stored in encKeys
   for (const QString& encKey: walletInfo->encKeys()) {
      auto deviceInfo = AutheIDClient::getDeviceInfo(SecureBinaryData(encKey.toStdString()).toBinStr());

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
         , walletInfo->rootId().toStdString(), knownDeviceIds, expiration, timestamp);
   }
   catch (const std::exception &e) {
      logger_->error("AuthEidClient failed to sign wallet: {}", e.what());
      QMetaObject::invokeMethod(this, [this, e](){
         emit failed(QString::fromStdString(e.what()));
      },
      Qt::QueuedConnection);
   }
}

void AuthSignWalletObject::removeDevice(int index, bs::hd::WalletInfo *walletInfo)
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

      auto deviceInfo = AutheIDClient::getDeviceInfo(SecureBinaryData(walletInfo->encKeys().at(i).toStdString()).toBinStr());

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
         , walletInfo->rootId().toStdString(), knownDeviceIds);
   }
   catch (const std::exception &e) {
      logger_->error("AuthEidClient failed to sign wallet: {}", e.what());
      QMetaObject::invokeMethod(this, [this, e](){
         emit failed(QString::fromStdString(e.what()));
      },
      Qt::QueuedConnection);
   }
}

