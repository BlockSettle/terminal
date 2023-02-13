/*

***********************************************************************************
* Copyright (C) 2020 - 2021, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __SIGNER_SETTINGS_H__
#define __SIGNER_SETTINGS_H__

#include <QStringList>
#include <QVariant>
#include "Wallets/SignerDefs.h"
#include "Wallets/SignerUiDefs.h"

namespace Blocksettle { namespace Communication { namespace signer {
   class Settings;
} } }
class HeadlessSettings;

//SignerSettings should be separated to headless and GUI parts once all the communication will be stabilized
class SignerSettings : public QObject
{
   Q_OBJECT
   Q_PROPERTY(bool offline READ offline WRITE setOffline NOTIFY offlineChanged)
   Q_PROPERTY(bool testNet READ testNet WRITE setTestNet NOTIFY testNetChanged)
   Q_PROPERTY(bool watchingOnly READ watchingOnly WRITE setWatchingOnly NOTIFY woChanged)
   Q_PROPERTY(QString exportWalletsDir READ getExportWalletsDir WRITE setExportWalletsDir NOTIFY exportWalletsDirChanged)
   Q_PROPERTY(QString listenAddress READ listenAddress WRITE setListenAddress NOTIFY listenSocketChanged)
   Q_PROPERTY(QString acceptFrom READ acceptFrom WRITE setAcceptFrom NOTIFY listenSocketChanged)
   Q_PROPERTY(QString listenPort READ port WRITE setPort NOTIFY listenSocketChanged)
   Q_PROPERTY(bool autoSignUnlimited READ autoSignUnlimited NOTIFY limitAutoSignXbtChanged)
   Q_PROPERTY(bool manualSignUnlimited READ manualSignUnlimited NOTIFY limitManualXbtChanged)
   Q_PROPERTY(double limitManualXbt READ limitManualXbt WRITE setLimitManualXbt NOTIFY limitManualXbtChanged)
   Q_PROPERTY(double limitAutoSignXbt READ limitAutoSignXbt WRITE setLimitAutoSignXbt NOTIFY limitAutoSignXbtChanged)
   Q_PROPERTY(QString limitAutoSignTime READ limitAutoSignTimeStr WRITE setLimitAutoSignTimeStr NOTIFY limitAutoSignTimeChanged)
   Q_PROPERTY(QString limitManualPwKeep READ limitManualPwKeepStr WRITE setLimitManualPwKeepStr NOTIFY limitManualPwKeepChanged)
   Q_PROPERTY(QString dirDocuments READ dirDocuments NOTIFY dirDocumentsChanged)
   Q_PROPERTY(QString autoSignWallet READ autoSignWallet WRITE setAutoSignWallet NOTIFY autoSignWalletChanged)
   Q_PROPERTY(bool hideEidInfoBox READ hideEidInfoBox WRITE setHideEidInfoBox NOTIFY hideEidInfoBoxChanged)
   Q_PROPERTY(QStringList trustedTerminals READ trustedTerminals WRITE setTrustedTerminals NOTIFY trustedTerminalsChanged)
   Q_PROPERTY(bool twoWaySignerAuth READ twoWaySignerAuth WRITE setTwoWaySignerAuth NOTIFY twoWaySignerAuthChanged)

public:
   SignerSettings();
   ~SignerSettings() override;

   SignerSettings(const SignerSettings&) = delete;
   SignerSettings& operator = (const SignerSettings&) = delete;
   SignerSettings(SignerSettings&&) = delete;
   SignerSettings& operator = (SignerSettings&&) = delete;

   bool loadSettings(const std::shared_ptr<HeadlessSettings> &);

   QString serverIDKeyStr() const;

   QString listenAddress() const;
   QString acceptFrom() const;
   QString port() const;
   QString logFileName() const;
   bool testNet() const;
   NetworkType netType() const { return (testNet() ? NetworkType::TestNet : NetworkType::MainNet); }
   bool watchingOnly() const;
   bool getSrvIDKeyBin(BinaryData& keyBuf);
   QString getExportWalletsDir() const;
   QString autoSignWallet() const;
   bool offline() const;
   double limitManualXbt() const;
   double limitAutoSignXbt() const;
   bool autoSignUnlimited() const;
   bool manualSignUnlimited() const;
   int limitAutoSignTime() const;
   QString limitAutoSignTimeStr() const;
   QString limitManualPwKeepStr() const;
   bs::signer::Limits limits() const;
   bool hideEidInfoBox() const;
   QStringList trustedTerminals() const;
   bool twoWaySignerAuth() const;

   QString dirDocuments() const;
   bool closeHeadless() const { return true; }

   void setOffline(bool val);
   void setTestNet(bool val);
   void setWatchingOnly(const bool val);
   void setExportWalletsDir(const QString &);
   void setAutoSignWallet(const QString &val);
   void setListenAddress(const QString &val);
   void setAcceptFrom(const QString &val);
   void setPort(const QString &val);
   void setLimitManualXbt(const double val);
   void setLimitAutoSignXbt(const double val);
   void setLimitAutoSignTimeStr(const QString &val);
   void setLimitManualPwKeepStr(const QString &val);
   void setHideEidInfoBox(bool val);
   void setTrustedTerminals(const QStringList &val);
   void setTwoWaySignerAuth(bool val);
   using Settings = Blocksettle::Communication::signer::Settings;
   const std::unique_ptr<Settings> &get() const { return d_; }

   static QString secondsToIntervalStr(int);
   static int intervalStrToSeconds(const QString &);

   int signerPort() { return signerPort_; }

signals:
   void offlineChanged();
   void testNetChanged();
   void woChanged();
   void exportWalletsDirChanged();
   void listenSocketChanged();
   void limitManualXbtChanged();
   void limitAutoSignXbtChanged();
   void limitAutoSignTimeChanged();
   void limitManualPwKeepChanged();
   void dirDocumentsChanged();
   void autoSignWalletChanged();
   void hideEidInfoBoxChanged();
   void trustedTerminalsChanged();
   void twoWaySignerAuthChanged();
   void changed(int);

private:
   void settingChanged(int setting);
   bool verifyServerIDKey();

   void setStringSetting(const QString &val, std::string *oldValue, int setting);

   std::string writableDir_;
   std::string fileName_;
   std::string srvIDKey_;
   int signerPort_{};
   std::unique_ptr<Settings> d_;
};


#endif // __SIGNER_SETTINGS_H__
