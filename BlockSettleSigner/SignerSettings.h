#ifndef __SIGNER_SETTINGS_H__
#define __SIGNER_SETTINGS_H__

#include <QStringList>
#include <QVariant>
#include "SignContainer.h"

class QSettings;


class SignerSettings : public QObject
{
   Q_OBJECT
   Q_PROPERTY(bool offline READ offline WRITE setOffline NOTIFY offlineChanged)
   Q_PROPERTY(bool testNet READ testNet WRITE setTestNet NOTIFY testNetChanged)
   Q_PROPERTY(QString walletsDir READ getWalletsDir WRITE setWalletsDir NOTIFY walletsDirChanged)
   Q_PROPERTY(QString listenAddress READ listenAddress WRITE setListenAddress NOTIFY listenSocketChanged)
   Q_PROPERTY(QString listenPort READ port WRITE setPort NOTIFY listenSocketChanged)
   Q_PROPERTY(QString password READ pwHash WRITE setPassword NOTIFY passwordChanged)
   Q_PROPERTY(bool autoSignUnlimited READ autoSignUnlimited NOTIFY limitAutoSignXbtChanged)
   Q_PROPERTY(bool manualSignUnlimited READ manualSignUnlimited NOTIFY limitManualXbtChanged)
   Q_PROPERTY(double limitManualXbt READ limitManualXbt WRITE setLimitManualXbt NOTIFY limitManualXbtChanged)
   Q_PROPERTY(double limitAutoSignXbt READ limitAutoSignXbt WRITE setLimitAutoSignXbt NOTIFY limitAutoSignXbtChanged)
   Q_PROPERTY(QString limitAutoSignTime READ limitAutoSignTimeStr WRITE setLimitAutoSignTimeStr NOTIFY limitAutoSignTimeChanged)
   Q_PROPERTY(QString limitManualPwKeep READ limitManualPwKeepStr WRITE setLimitManualPwKeepStr NOTIFY limitManualPwKeepChanged)
   Q_PROPERTY(QString dirDocuments READ dirDocuments NOTIFY dirDocumentsChanged)
   Q_PROPERTY(QString autoSignWallet READ autoSignWallet WRITE setAutoSignWallet NOTIFY autoSignWalletChanged)

public:
   SignerSettings(const QStringList &args = {}, const QString &fileName = QLatin1String("signer.ini"));

   SignerSettings(const SignerSettings&) = delete;
   SignerSettings& operator = (const SignerSettings&) = delete;
   SignerSettings(SignerSettings&&) = delete;
   SignerSettings& operator = (SignerSettings&&) = delete;

   enum Setting {
      OfflineMode,
      TestNet,
      WalletsDir,
      AutoSignWallet,
      LogFileName,
      ListenAddress,
      ListenPort,
      ConnPubKey,
      ConnPrivKey,
      PasswordHash,
      LimitManualXBT,
      LimitAutoSignXBT,
      LimitAutoSignTime,
      LimitManualPwKeep
   };

   QString publicKey() const { return get(ConnPubKey).toString(); }
   QString privateKey() const { return get(ConnPrivKey).toString(); }
   QString listenAddress() const { return get(ListenAddress).toString(); }
   QString port() const { return get(ListenPort).toString(); }
   QString logFileName() const { return get(LogFileName).toString(); }
   bool testNet() const { return get(TestNet).toBool(); }
   NetworkType netType() const { return (testNet() ? NetworkType::TestNet : NetworkType::MainNet); }
   QString getWalletsDir() const;
   QString autoSignWallet() const { return get(AutoSignWallet).toString(); }
   QString pwHash() const { return get(PasswordHash).toString(); }
   bool offline() const { return get(OfflineMode).toBool(); }
   double limitManualXbt() const { return get(LimitManualXBT).toULongLong() / BTCNumericTypes::BalanceDivider; }
   double limitAutoSignXbt() const { return get(LimitAutoSignXBT).toULongLong() / BTCNumericTypes::BalanceDivider; }
   bool autoSignUnlimited() const { return (limits().autoSignSpendXBT > (UINT64_MAX / 2)); }
   bool manualSignUnlimited() const { return (limits().manualSpendXBT > (UINT64_MAX / 2)); }
   int limitAutoSignTime() const { return get(LimitAutoSignTime).toInt(); }
   QString limitAutoSignTimeStr() const { return secondsToIntervalStr(limitAutoSignTime()); }
   int limitManualPwKeep() const { return get(LimitManualPwKeep).toInt(); }
   QString limitManualPwKeepStr() const { return secondsToIntervalStr(limitManualPwKeep()); }
   QStringList requestFiles() const { return reqFiles_; }
   SignContainer::Limits limits() const;

   QString dirDocuments() const;

   void setOffline(const bool val = true) { set(OfflineMode, val); }
   void setTestNet(const bool val) { set(TestNet, val); }
   void setWalletsDir(const QString &);
   void setAutoSignWallet(const QString &val) { set(AutoSignWallet, val); }
   void setListenAddress(const QString &val) { set(ListenAddress, val); }
   void setPort(const QString &val) { set(ListenPort, val); }
   void setPassword(const QString &password);
   void setLimitManualXbt(const double val) { setXbtLimit(val, LimitManualXBT); }
   void setLimitAutoSignXbt(const double val) { setXbtLimit(val, LimitAutoSignXBT); }
   void setLimitAutoSignTimeStr(const QString &val) { set(LimitAutoSignTime, intervalStrToSeconds(val)); }
   void setLimitManualPwKeepStr(const QString &val) { set(LimitManualPwKeep, intervalStrToSeconds(val)); }

   void reset(Setting s, bool toFile = true);     // Reset setting to default value

   static QString secondsToIntervalStr(int);
   static int intervalStrToSeconds(const QString &);

signals:
   void offlineChanged();
   void testNetChanged();
   void walletsDirChanged();
   void listenSocketChanged();
   void passwordChanged();
   void limitManualXbtChanged();
   void limitAutoSignXbtChanged();
   void limitAutoSignTimeChanged();
   void limitManualPwKeepChanged();
   void dirDocumentsChanged();
   void autoSignWalletChanged();

private:
   QVariant get(Setting s) const;
   void set(Setting s, const QVariant &val, bool toFile = true);

   void parseArguments(const QStringList &args);
   void settingChanged(Setting, const QVariant &val);
   void setXbtLimit(const double val, Setting);

private:
   struct SettingDef {
      QString  path;
      QVariant defVal;
      mutable bool     read;
      mutable QVariant value;

      SettingDef(const QString &_path, const QVariant &_defVal = QVariant())
         : path(_path), defVal(_defVal), read(false) {}
   };

   std::map<Setting, SettingDef> settingDefs_;
   std::shared_ptr<QSettings>    backend_;
   std::string    writableDir_;
   QStringList    reqFiles_;
};

#endif // __SIGNER_SETTINGS_H__
