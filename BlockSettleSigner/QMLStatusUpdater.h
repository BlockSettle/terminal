/*

***********************************************************************************
* Copyright (C) 2018 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __QML_STATUS_UPDATER_H__
#define __QML_STATUS_UPDATER_H__

#include <memory>
#include <QObject>
#include <QTimer>
#include <QElapsedTimer>
#include <QJSValue>
#include <Wallets/QPasswordData.h>
#include "Settings/SignerSettings.h"

namespace spdlog {
   class logger;
}
class SignerAdapter;


class QMLStatusUpdater : public QObject
{
   Q_OBJECT
   Q_PROPERTY(bool offline READ offline NOTIFY offlineChanged)
   Q_PROPERTY(int connections READ connections NOTIFY connectionsChanged)
   Q_PROPERTY(bool autoSignUnlimited READ autoSignUnlimited NOTIFY autoSignLimitChanged)
   Q_PROPERTY(bool manualSignUnlimited READ manualSignUnlimited NOTIFY manualSignLimitChanged)
   Q_PROPERTY(double autoSignLimit READ autoSignLimit NOTIFY autoSignLimitChanged)
   Q_PROPERTY(double manualSignLimit READ manualSignLimit NOTIFY manualSignLimitChanged)
   Q_PROPERTY(QString autoSignTimeLimit READ autoSignTimeLimit NOTIFY autoSignTimeLimitChanged)
   Q_PROPERTY(int txSignedCount READ txSignedCount NOTIFY txSignedCountChanged)
   Q_PROPERTY(double autoSignSpent READ autoSignSpent NOTIFY autoSignSpentChanged)
   Q_PROPERTY(double manualSignSpent READ manualSignSpent NOTIFY manualSignSpentChanged)
   Q_PROPERTY(QString autoSignTimeSpent READ autoSignTimeSpent NOTIFY autoSignTimeSpentChanged)
   Q_PROPERTY(QString listenSocket READ listenSocket NOTIFY listenSocketChanged)
   Q_PROPERTY(bool autoSignActive READ autoSignActive NOTIFY autoSignActiveChanged)
   Q_PROPERTY(bool socketOk READ socketOk NOTIFY socketOkChanged)
   Q_PROPERTY(QStringList connectedClients READ connectedClients NOTIFY connectionsChanged)

public:
   QMLStatusUpdater(const std::shared_ptr<SignerSettings> &, SignerAdapter *
      , const std::shared_ptr<spdlog::logger> &);

   void setSocketOk(bool);
   void clearConnections();

   Q_INVOKABLE void activateAutoSign(const QString &walletId
      , bs::wallet::QPasswordData *passwordData
      , bool activate
      , QJSValue jsCallback);

signals:
   void offlineChanged();
   void connectionsChanged();
   void autoSignLimitChanged();
   void manualSignLimitChanged();
   void autoSignTimeLimitChanged();
   void txSignedCountChanged();
   void autoSignSpentChanged();
   void manualSignSpentChanged();
   void autoSignTimeSpentChanged();
   void listenSocketChanged();
   void autoSignActiveChanged();
   void socketOkChanged();

private slots:
   void txSigned(const BinaryData &);
   void xbtSpent(const qint64 value, bool autoSign);
   void onAutoSignActivated(const std::string &walletId);
   void onAutoSignDeactivated(const std::string &walletId);
   void onAutoSignTick();
   void onPeerConnected(const std::string &clientId, const std::string &ip, const std::string &publicKey);
   void onPeerDisconnected(const std::string &clientId);
   QJSValue invokeJsCallBack(QJSValue jsCallback, QJSValueList args);

private:
   bool offline() const { return settings_->offline(); }
   int connections() const { return connectedClientIps_.size(); }
   QStringList connectedClients() const;
   bool autoSignUnlimited() const { return settings_->autoSignUnlimited(); }
   bool manualSignUnlimited() const { return settings_->manualSignUnlimited(); }
   double autoSignLimit() const { return settings_->limitAutoSignXbt(); }
   double manualSignLimit() const { return settings_->limitManualXbt(); }
   int timeAutoSignSeconds() const;
   QString autoSignTimeLimit() const { return settings_->limitAutoSignTimeStr(); }
   QString autoSignTimeSpent() const;
   int txSignedCount() const { return txSignedCount_; }
   double autoSignSpent() const { return autoSignSpent_ / BTCNumericTypes::BalanceDivider; }
   double manualSignSpent() const { return manualSignSpent_ / BTCNumericTypes::BalanceDivider; }
   QString listenSocket() const { return settings_->listenAddress() + QLatin1String(" : ") + settings_->port(); }
   bool autoSignActive() const { return autoSignActive_; }
   bool socketOk() const { return socketOk_; }

private:
   std::shared_ptr<SignerSettings>  settings_;
   SignerAdapter  *  adapter_;
   std::shared_ptr<spdlog::logger>  logger_;
   QTimer   asTimer_;
   int      txSignedCount_ = 0;
   uint64_t autoSignSpent_ = 0;
   uint64_t manualSignSpent_ = 0;
   QElapsedTimer autoSignTimeSpent_;
   bool     autoSignActive_ = false;
   bool     socketOk_ = true;
   std::map<std::string, std::string> connectedClientIps_;
};

#endif // __QML_STATUS_UPDATER_H__
