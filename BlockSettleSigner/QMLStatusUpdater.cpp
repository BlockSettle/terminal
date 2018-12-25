#include "QMLStatusUpdater.h"
#include "HeadlessContainerListener.h"


QMLStatusUpdater::QMLStatusUpdater(const std::shared_ptr<SignerSettings> &params)
   : QObject(nullptr), settings_(params)
{
   connect(settings_.get(), &SignerSettings::offlineChanged, [this] { emit offlineChanged(); });
   connect(settings_.get(), &SignerSettings::listenSocketChanged, [this] { emit listenSocketChanged(); });
   connect(settings_.get(), &SignerSettings::limitManualXbtChanged, [this] { emit manualSignLimitChanged(); });
   connect(settings_.get(), &SignerSettings::limitAutoSignXbtChanged, [this] { emit autoSignLimitChanged(); });
   connect(settings_.get(), &SignerSettings::limitAutoSignTimeChanged, [this] { emit autoSignTimeLimitChanged(); });

   connect(&asTimer_, &QTimer::timeout, this, &QMLStatusUpdater::onAutoSignTick);
}

void QMLStatusUpdater::SetListener(const std::shared_ptr<HeadlessContainerListener> &listener)
{
   listener_ = listener;
   if (listener_) {
      connect(listener_.get(), &HeadlessContainerListener::peerConnected,
         this, &QMLStatusUpdater::onPeerConnected);
      connect(listener_.get(), &HeadlessContainerListener::peerDisconnected,
         this, &QMLStatusUpdater::onPeerDisconnected);
      connect(listener_.get(), &HeadlessContainerListener::txSigned, this, &QMLStatusUpdater::txSigned);
      connect(listener_.get(), &HeadlessContainerListener::xbtSpent, this, &QMLStatusUpdater::xbtSpent);
      connect(listener_.get(), &HeadlessContainerListener::autoSignActivated, this, &QMLStatusUpdater::onAutoSignActivated);
      connect(listener_.get(), &HeadlessContainerListener::autoSignDeactivated, this, &QMLStatusUpdater::onAutoSignDeactivated);
   }
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
   if (listener_) {
      listener_->deactivateAutoSign({});
   }
}

void QMLStatusUpdater::activateAutoSign()
{
   if (listener_) {
      emit autoSignActiveChanged();
      const auto &walletId = settings_->autoSignWallet().toStdString();
      listener_->addPendingAutoSignReq(walletId);
      emit autoSignRequiresPwd(walletId);
   }
}

void QMLStatusUpdater::onAutoSignActivated(const std::string &walletId)
{
   autoSignActive_ = true;
   emit autoSignActiveChanged();
   autoSignTimeSpent_ = 0;
   asTimer_.start(1000);
}

void QMLStatusUpdater::onAutoSignDeactivated(const std::string &walletId)
{
   autoSignActive_ = false;
   emit autoSignActiveChanged();
   asTimer_.stop();
   autoSignTimeSpent_ = 0;
   emit autoSignTimeSpentChanged();
}

void QMLStatusUpdater::onAutoSignTick()
{
   autoSignTimeSpent_++;
   emit autoSignTimeSpentChanged();

   if ((settings_->limitAutoSignTime() > 0) && (autoSignTimeSpent_ >= settings_->limitAutoSignTime())) {
      deactivateAutoSign();
   }
}

void QMLStatusUpdater::onPeerConnected(const QString &ip)
{
   connectedClients_.insert(ip);
   emit connectionsChanged();
}

void QMLStatusUpdater::onPeerDisconnected(const QString &ip)
{
   connectedClients_.erase(ip);
   emit connectionsChanged();
}

void QMLStatusUpdater::txSigned()
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
