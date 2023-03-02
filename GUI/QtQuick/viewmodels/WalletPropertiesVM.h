/*

***********************************************************************************
* Copyright (C) 2023, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#pragma once

#include <QObject>

namespace qtquick_gui
{

struct WalletInfo
{
   QString name;
   QString description;
   QString walletId;
   QString groups;
   QString ecryption;
   quint32 generatedAddresses;
   bool isHardware;
   bool isWatchingOnly;
};

class WalletPropertiesVM: public QObject
{
   Q_OBJECT
   Q_PROPERTY(QString walletName               READ walletName               NOTIFY changed)
   Q_PROPERTY(QString walletDescription        READ walletDescription        NOTIFY changed)
   Q_PROPERTY(QString walletId                 READ walletId                 NOTIFY changed)
   Q_PROPERTY(QString walletGroups             READ walletGroups             NOTIFY changed)
   Q_PROPERTY(QString walletEncryption         READ walletEncryption         NOTIFY changed)
   Q_PROPERTY(quint32 walletGeneratedAddresses READ walletGeneratedAddresses NOTIFY changed)
   Q_PROPERTY(quint32 walletActiveAddresses    READ walletActiveAddresses    NOTIFY changed)
   Q_PROPERTY(quint32 walletAvailableUtxo      READ walletAvailableUtxo      NOTIFY changed)
   Q_PROPERTY(bool isHardware                  READ isHardware               NOTIFY changed)
   Q_PROPERTY(bool isWatchingOnly              READ isWatchingOnly           NOTIFY changed)
   Q_PROPERTY(QStringList seed                 READ seed                     NOTIFY seedChanged)
   Q_PROPERTY(QString exportPath READ exportPath WRITE setExportPath NOTIFY pathChanged)

public:
   WalletPropertiesVM(QObject* parent = nullptr);

   void setWalletInfo(const WalletInfo& info);
   void setWalletSeed(const std::string& walletId, const std::string& seed);
   void setNbActiveAddrs(const std::string& walletId, uint32_t nb);
   void setNbUTXOs(const std::string& walletId, uint32_t nb);

   const QString& walletName() const;
   const QString& walletDescription() const;
   const QString& walletId() const;
   const QString& walletGroups() const;
   const QString& walletEncryption() const;
   quint32 walletGeneratedAddresses() const;
   quint32 walletActiveAddresses() const;
   quint32 walletAvailableUtxo() const;
   bool isHardware() const;
   bool isWatchingOnly() const;

   const QStringList& seed() const;

   const QString& exportPath() const;
   void setExportPath(const QString& path);

signals:
   void changed();
   void seedChanged();
   void pathChanged();

private:
   WalletInfo  info_;
   uint32_t    nbActiveAddrs_{ 0 };
   uint32_t    nbUTXOs_{ 0 };
   QStringList seed_;
   QString exportPath_;
};

}
