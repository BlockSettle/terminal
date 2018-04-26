#ifndef __QML_STATUS_UPDATER_H__
#define __QML_STATUS_UPDATER_H__

#include <memory>
#include <QObject>
#include <QTimer>
#include "SignerSettings.h"

class HeadlessContainerListener;


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
   QMLStatusUpdater(const std::shared_ptr<SignerSettings> &);

   void SetListener(const std::shared_ptr<HeadlessContainerListener> &);

   void setSocketOk(bool);
   void clearConnections();

   Q_INVOKABLE void deactivateAutoSign();
   Q_INVOKABLE void activateAutoSign();

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
   void autoSignRequiresPwd(const std::string &walletId);

private slots:
   void connected(const std::string &clientId, const std::string &clientInfo);
   void disconnected(const std::string &clientId);
   void txSigned();
   void xbtSpent(const qint64 value, bool autoSign);
   void onAutoSignActivated(const std::string &walletId);
   void onAutoSignDeactivated(const std::string &walletId);
   void onAutoSignTick();

private:
   bool offline() const { return params_->offline(); }
   int connections() const { return connectedClients_.size(); }
   QStringList connectedClients() const;
   bool autoSignUnlimited() const { return params_->autoSignUnlimited(); }
   bool manualSignUnlimited() const { return params_->manualSignUnlimited(); }
   double autoSignLimit() const { return params_->limitAutoSignXbt(); }
   double manualSignLimit() const { return params_->limitManualXbt(); }
   QString autoSignTimeLimit() const { return params_->limitAutoSignTimeStr(); }
   QString autoSignTimeSpent() const { return SignerSettings::secondsToIntervalStr(autoSignTimeSpent_); }
   int txSignedCount() const { return txSignedCount_; }
   double autoSignSpent() const { return autoSignSpent_ / BTCNumericTypes::BalanceDivider; }
   double manualSignSpent() const { return manualSignSpent_ / BTCNumericTypes::BalanceDivider; }
   QString listenSocket() const { return params_->listenAddress() + QLatin1String(" : ") + params_->port(); }
   bool autoSignActive() const { return autoSignActive_; }
   bool socketOk() const { return socketOk_; }

private:
   std::shared_ptr<SignerSettings>  params_;
   std::shared_ptr<HeadlessContainerListener>   listener_;
   QTimer   asTimer_;
   int      txSignedCount_ = 0;
   uint64_t autoSignSpent_ = 0;
   uint64_t manualSignSpent_ = 0;
   int      autoSignTimeSpent_ = 0;
   bool     autoSignActive_ = false;
   bool     socketOk_ = true;
   std::unordered_map<std::string, QString>  connectedClients_;
};

#endif // __QML_STATUS_UPDATER_H__
