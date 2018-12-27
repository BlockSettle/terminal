#include <QFile>
#include <QVariant>
#include <QBuffer>
#include <QByteArray>
#include <QPixmap>

#include <spdlog/spdlog.h>

#include "AuthProxy.h"
#include "ApplicationSettings.h"


void AuthSignWalletObject::cancel()
{
   if (autheIDClient_) {
      autheIDClient_->cancel();
   }
}


void AuthObject::setStatus(const QString &status)
{
   status_ = tr("Auth status: %1").arg(status);
   emit statusChanged();
}


AuthSignWalletObject::AuthSignWalletObject(const std::shared_ptr<spdlog::logger> &logger, QObject *parent)
   : AuthObject(nullptr)
{
   ApplicationSettings settings;
   auto authKeys = settings.GetAuthKeys();
   autheIDClient_ = (new AutheIDClient(logger, authKeys, this));

   connect(autheIDClient_, &AutheIDClient::succeeded, this, [this](const std::string &encKey, const SecureBinaryData &password){
       emit succeeded(QString::fromStdString(encKey), password);
   });
   connect(autheIDClient_, &AutheIDClient::failed, this, [this](const QString &text){
       emit failed(text);
   });
   std::string serverPubKey = settings.get<std::string>(ApplicationSettings::authServerPubKey);
   std::string serverHost = settings.get<std::string>(ApplicationSettings::authServerHost);
   std::string serverPort = settings.get<std::string>(ApplicationSettings::authServerPort);

   autheIDClient_->connect(serverPubKey, serverHost, serverPort);
}



bool AuthSignWalletObject::signWallet(AutheIDClient::RequestType requestType, bs::hd::WalletInfo *walletInfo)
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

   if (userIds.empty()) {
      //emit failed(tr("Error parsing encKeys: email not found"));
      return false;
   }

   autheIDClient_->start(requestType
                         , userIds[0]
                         , walletInfo->walletId().toStdString()
                         , knownDeviceIds);
   return true;
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

   if (userIds.empty()) {
      emit failed(tr("Error parsing encKeys: email not found"));
      return;
   }

   // currently we supports only sigle account for whole wallet, thus email stored in userIds[0]
   autheIDClient_->start(AutheIDClient::DeactivateWalletDevice
                         , userIds[0]
                         , walletInfo->walletId().toStdString()
                         , knownDeviceIds);
}

