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
#include "Wallets/SignContainer.h"

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
   quint32 activeAddresses;
   quint32 availableUtxo;
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
   Q_PROPERTY(QStringList seed                 READ seed                     NOTIFY seedChanged)
   Q_PROPERTY(QString exportPath READ exportPath WRITE setExportPath NOTIFY pathChanged)

public:
   WalletPropertiesVM(QObject* parent = nullptr);

   void setWalletInfo(const WalletInfo& info);

   const QString& walletName() const;
   const QString& walletDescription() const;
   const QString& walletId() const;
   const QString& walletGroups() const;
   const QString& walletEncryption() const;
   quint32 walletGeneratedAddresses() const;
   quint32 walletActiveAddresses() const;
   quint32 walletAvailableUtxo() const;

   const QStringList& seed() const;
   void setSeed(const QStringList& seed);

   const QString& exportPath() const;
   void setExportPath(const QString& path);

   Q_INVOKABLE int changePassword(const QString& oldPassword, const QString& newPassword);
   Q_INVOKABLE int exportWalletAuth(const QString& password);
   Q_INVOKABLE int viewWalletSeedAuth(const QString& password);
   Q_INVOKABLE int deleteWallet(const QString& password);
   Q_INVOKABLE int exportWallet();

signals:
   void changed();
   void seedChanged();
   void pathChanged();

private:
   WalletInfo info_;
   QStringList seed_;
   QString exportPath_;
};

}
