#include "QMLStatusUpdater.h"
#include <spdlog/spdlog.h>
#include "HeadlessContainerListener.h"
#include "SignerAdapter.h"


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
   connectedClients_.clear();
   emit connectionsChanged();
}

void QMLStatusUpdater::deactivateAutoSign()
{
   adapter_->deactivateAutoSign();
}

void QMLStatusUpdater::activateAutoSign(const QString &walletId
                                        , bs::wallet::QPasswordData passwordData
                                        , bool activate
                                        , QJSValue jsCallback)
{
   emit autoSignActiveChanged();
   auto cb = [this, walletId, jsCallback] (bool success) {
      QJSValueList args;
      args << QJSValue(success);
      QMetaObject::invokeMethod(this, [this, args, jsc=jsCallback] {
         if (jsc.isCallable()) {
            return jsc.call(args);
         }
      });
   };

   adapter_->activateAutoSign(walletId, passwordData, true, cb);
   emit autoSignRequiresPwd(walletId);
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
   emit autoSignTimeSpentChanged();
}

void QMLStatusUpdater::onAutoSignTick()
{
   emit autoSignTimeSpentChanged();

   if ((settings_->limitAutoSignTime() > 0) && (timeAutoSignSeconds() >= settings_->limitAutoSignTime())) {
      deactivateAutoSign();
   }
}

void QMLStatusUpdater::onPeerConnected(const QString &ip)
{
   logger_->debug("[{}] {}", __func__, ip.toStdString());
   connectedClients_.insert(ip);
   emit connectionsChanged();
}

void QMLStatusUpdater::onPeerDisconnected(const QString &ip)
{
   logger_->debug("[{}] {}", __func__, ip.toStdString());
   connectedClients_.erase(ip);
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
   for (const auto &client : connectedClients_) {
      result.push_back(client);
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
