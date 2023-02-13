/*

***********************************************************************************
* Copyright (C) 2023, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "WalletPropertiesVM.h"
#include <QDebug>

namespace qtquick_gui
{

WalletPropertiesVM::WalletPropertiesVM(QObject* parent)
   : QObject(parent)
{
}

void WalletPropertiesVM::setWalletInfo(const WalletInfo& info)
{
   info_ = info;
   emit changed();
}

const QString& WalletPropertiesVM::walletName() const
{
   return info_.name;
}

const QString& WalletPropertiesVM::walletDescription() const
{
   return info_.description;
}

const QString& WalletPropertiesVM::walletId() const
{
   return info_.walletId;
}

const QString& WalletPropertiesVM::walletGroups() const
{
   return info_.groups;
}

const QString& WalletPropertiesVM::walletEncryption() const
{
   return info_.ecryption;
}

quint32 WalletPropertiesVM::walletGeneratedAddresses() const
{
   return info_.generatedAddresses;
}

quint32 WalletPropertiesVM::walletActiveAddresses() const
{
   return info_.activeAddresses;
}

quint32 WalletPropertiesVM::walletAvailableUtxo() const
{
   return info_.availableUtxo;
}

int WalletPropertiesVM::changePassword(const QString& oldPassword, const QString& newPassword)
{
   qDebug() << "Changed pwd request:" << oldPassword << " " << newPassword;
   return 0;
}

int WalletPropertiesVM::exportWalletAuth(const QString& password)
{
   qDebug() << "export WO wallet auth:" << password;
   return 0;
}

int WalletPropertiesVM::viewWalletSeedAuth(const QString& password)
{
   qDebug() << "view wallet seed:" << password;
   return 0;
}

int WalletPropertiesVM::deleteWallet(const QString& password)
{
   qDebug() << "delete wallet:" << password;
   return 0;
}

const QStringList& WalletPropertiesVM::seed() const
{
   return seed_;
}

void WalletPropertiesVM::setSeed(const QStringList& seed)
{
   seed_ = seed;
   emit seedChanged();
}


const QString& WalletPropertiesVM::exportPath() const
{
   return exportPath_;
}

void WalletPropertiesVM::setExportPath(const QString& path)
{
   exportPath_ = path;
   emit pathChanged();
}

int WalletPropertiesVM::exportWallet()
{
   qDebug() << "Exporting wallet to " << exportPath_;
   return 0;
}

}
