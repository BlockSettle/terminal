#include "QMLStatusUpdater.h"
#include "HeadlessContainerListener.h"


QMLStatusUpdater::QMLStatusUpdater(const std::shared_ptr<SignerSettings> &params)
   : QObject(nullptr), params_(params)
{
   connect(params_.get(), &SignerSettings::offlineChanged, [this] { emit offlineChanged(); });
   connect(params_.get(), &SignerSettings::listenSocketChanged, [this] { emit listenSocketChanged(); });
   connect(params_.get(), &SignerSettings::limitManualXbtChanged, [this] { emit manualSignLimitChanged(); });
   connect(params_.get(), &SignerSettings::limitAutoSignXbtChanged, [this] { emit autoSignLimitChanged(); });
   connect(params_.get(), &SignerSettings::limitAutoSignTimeChanged, [this] { emit autoSignTimeLimitChanged(); });

   connect(&asTimer_, &QTimer::timeout, this, &QMLStatusUpdater::onAutoSignTick);
}

void QMLStatusUpdater::SetListener(const std::shared_ptr<HeadlessContainerListener> &listener)
{
   listener_ = listener;
   if (listener_) {
      connect(listener_.get(), &HeadlessContainerListener::clientAuthenticated, this, &QMLStatusUpdater::connected);
      connect(listener_.get(), &HeadlessContainerListener::clientDisconnected, this, &QMLStatusUpdater::disconnected);
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
      const auto &walletId = params_->autoSignWallet().toStdString();
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

   if ((params_->limitAutoSignTime() > 0) && (autoSignTimeSpent_ >= params_->limitAutoSignTime())) {
      deactivateAutoSign();
   }
}

void QMLStatusUpdater::connected(const std::string &clientId, const std::string &clientInfo)
{
   connectedClients_[clientId] = QString::fromStdString(clientInfo);
   emit connectionsChanged();
}

void QMLStatusUpdater::disconnected(const std::string &clientId)
{
   connectedClients_.erase(clientId);
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
      result.push_back(client.second);
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
