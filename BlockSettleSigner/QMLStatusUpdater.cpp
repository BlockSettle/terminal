/*

***********************************************************************************
* Copyright (C) 2018 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "QMLStatusUpdater.h"
#include <spdlog/spdlog.h>
#include "SignerAdapter.h"
#include "BSErrorCodeStrings.h"
#include "StringUtils.h"


QMLStatusUpdater::QMLStatusUpdater(const std::shared_ptr<SignerSettings> &params, SignerAdapter *adapter
   , const std::shared_ptr<spdlog::logger> &logger)
   : QObject(nullptr), settings_(params), adapter_(adapter), logger_(logger)
{
   connect(settings_.get(), &SignerSettings::offlineChanged, this, &QMLStatusUpdater::offlineChanged);
   connect(settings_.get(), &SignerSettings::listenSocketChanged, this, &QMLStatusUpdater::listenSocketChanged);
   connect(settings_.get(), &SignerSettings::limitManualXbtChanged, this, &QMLStatusUpdater::manualSignLimitChanged);
   connect(settings_.get(), &SignerSettings::limitAutoSignXbtChanged, this, &QMLStatusUpdater::autoSignLimitChanged);
   connect(settings_.get(), &SignerSettings::limitAutoSignTimeChanged, this, &QMLStatusUpdater::autoSignTimeLimitChanged);

   connect(adapter_, &SignerAdapter::peerConnected, this, &QMLStatusUpdater::onPeerConnected);
   connect(adapter_, &SignerAdapter::peerDisconnected, this, &QMLStatusUpdater::onPeerDisconnected);
   connect(adapter_, &SignerAdapter::txSigned, this, &QMLStatusUpdater::txSigned);
   connect(adapter_, &SignerAdapter::xbtSpent, this, &QMLStatusUpdater::xbtSpent);
   connect(adapter_, &SignerAdapter::autoSignActivated, this, &QMLStatusUpdater::onAutoSignActivated);
   connect(adapter_, &SignerAdapter::autoSignDeactivated, this, &QMLStatusUpdater::onAutoSignDeactivated);

   connect(&asTimer_, &QTimer::timeout, this, &QMLStatusUpdater::onAutoSignTick);
}

void QMLStatusUpdater::setSocketOk(bool val)
{
   if (val != socketOk_) {
      socketOk_ = val;
      emit socketOkChanged();
   }
}

void QMLStatusUpdater::clearConnections()
{
   connectedClientIps_.clear();
   emit connectionsChanged();
}

void QMLStatusUpdater::activateAutoSign(const QString &walletId
   , bs::wallet::QPasswordData *passwordData, bool activate, QJSValue jsCallback)
{
   auto cb = [this, walletId, jsCallback] (bs::error::ErrorCode errorCode) {
      QJSValueList args;
      args << QJSValue(errorCode == bs::error::ErrorCode::NoError) << bs::error::ErrorCodeToString(errorCode);
      QMetaObject::invokeMethod(this, [this, args, jsCallback] {
         invokeJsCallBack(jsCallback, args);
      });
   };

   adapter_->activateAutoSign(walletId.toStdString(), passwordData, activate, cb);
   //emit autoSignActiveChanged();
}

void QMLStatusUpdater::onAutoSignActivated(const std::string &walletId)
{
   autoSignActive_ = true;
   emit autoSignActiveChanged();
   autoSignTimeSpent_.start();
   asTimer_.start(1000);
}

void QMLStatusUpdater::onAutoSignDeactivated(const std::string &walletId)
{
   autoSignActive_ = false;
   emit autoSignActiveChanged();
   asTimer_.stop();
   autoSignTimeSpent_.invalidate();
   autoSignSpent_ = 0;
   emit autoSignTimeSpentChanged();
}

void QMLStatusUpdater::onAutoSignTick()
{
//   emit autoSignTimeSpentChanged();

//   if ((settings_->limitAutoSignTime() > 0) && (timeAutoSignSeconds() >= settings_->limitAutoSignTime())) {
//      deactivateAutoSign();
//   }
}

void QMLStatusUpdater::onPeerConnected(const std::string &clientId, const std::string &ip, const std::string &publicKey)
{
   logger_->debug("[{}] connected client {} with ip {}, public key: ", __func__, bs::toHex(clientId), ip);
   connectedClientIps_[clientId] = ip;
   emit connectionsChanged();
}

void QMLStatusUpdater::onPeerDisconnected(const std::string &clientId)
{
   logger_->debug("[{}] {}", __func__, bs::toHex(clientId));
   connectedClientIps_.erase(clientId);
   emit connectionsChanged();
}

void QMLStatusUpdater::txSigned(const BinaryData &)
{
   txSignedCount_++;
   emit txSignedCountChanged();
}

QStringList QMLStatusUpdater::connectedClients() const
{
   QStringList result;
   for (const auto &client : connectedClientIps_) {
      result.push_back(QString::fromStdString(client.second));
   }
   return result;
}

int QMLStatusUpdater::timeAutoSignSeconds() const
{
   if (!autoSignTimeSpent_.isValid()) {
      return 0;
   }
   return int(autoSignTimeSpent_.elapsed() / 1000);
}

QString QMLStatusUpdater::autoSignTimeSpent() const
{
   return SignerSettings::secondsToIntervalStr(timeAutoSignSeconds());
}

void QMLStatusUpdater::xbtSpent(const qint64 value, bool autoSign)
{
   if (autoSign) {
      autoSignSpent_ += value;
      emit autoSignSpentChanged();
   }
   else {
      manualSignSpent_ += value;
      emit manualSignSpentChanged();
   }
}

QJSValue QMLStatusUpdater::invokeJsCallBack(QJSValue jsCallback, QJSValueList args)
{
   if (jsCallback.isCallable()) {
      return jsCallback.call(args);
   }
   else {
      return QJSValue();
   }
}
