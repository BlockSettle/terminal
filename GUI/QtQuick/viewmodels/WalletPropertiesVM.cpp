/*

***********************************************************************************
* Copyright (C) 2023, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/

#include "WalletPropertiesVM.h"

#include <spdlog/spdlog.h>

using namespace qtquick_gui;

namespace {
   static inline QString walletTypeFromInfo(const bs::sync::WalletInfo& info)
   {
      if (info.isHardware) {
         return QObject::tr("Hardware");
      }
      if (info.watchOnly) {
         return QObject::tr("Watch-only");
      }
      return QObject::tr("Software");
   }
}

WalletPropertiesVM::WalletPropertiesVM(const std::shared_ptr<spdlog::logger> & logger, QObject* parent)
   : QObject(parent),
     logger_(logger)
{
}

void WalletPropertiesVM::setWalletInfo(const QString& walletId, const bs::sync::WalletInfo& info)
{
   info_.name = QString::fromStdString(info.name);
   info_.walletId = walletId;
   info_.groups = QString::fromLatin1("1/") + QString::number(info.leaves.size());
   info_.walletType = walletTypeFromInfo(info);
   info_.generatedAddresses = info.nbAddresses;
   info_.isHardware = info.isHardware;
   info_.isWatchingOnly = info.watchOnly;
   nbUsedAddrs_ = 0;
   nbUTXOs_ = 0;
   seed_.clear();
   emit changed();
}

void qtquick_gui::WalletPropertiesVM::setWalletSeed(const std::string& walletId, const std::string& seed)
{
   if (walletId != info_.walletId.toStdString()) {
      return;
   }
   seed_ = QString::fromStdString(seed).split(QLatin1Char(' '));
   emit seedChanged();
}

void qtquick_gui::WalletPropertiesVM::setNbUsedAddrs(const std::string& walletId, uint32_t nb)
{
   if (info_.walletId.isEmpty() || (info_.walletId.toStdString() != walletId)) {
      return;
   }
   nbUsedAddrs_ = nb;
   emit changed();
}

void qtquick_gui::WalletPropertiesVM::setNbUTXOs(const std::string& walletId, uint32_t nb)
{
   if (info_.walletId.isEmpty() || (info_.walletId.toStdString() != walletId)) {
      return;
   }
   nbUTXOs_ = nb;
   emit changed();
}

const QString& WalletPropertiesVM::walletName() const
{
   return info_.name;
}

const QString& WalletPropertiesVM::walletId() const
{
   return info_.walletId;
}

const QString& WalletPropertiesVM::walletGroups() const
{
   return info_.groups;
}

const QString& WalletPropertiesVM::walletType() const
{
   return info_.walletType;
}

quint32 WalletPropertiesVM::walletGeneratedAddresses() const
{
   return info_.generatedAddresses;
}

quint32 WalletPropertiesVM::walletUsedAddresses() const
{
   return nbUsedAddrs_;
}

quint32 WalletPropertiesVM::walletAvailableUtxo() const
{
   return nbUTXOs_;
}

bool WalletPropertiesVM::isHardware() const
{
   return info_.isHardware;
}

bool WalletPropertiesVM::isWatchingOnly() const
{
   return info_.isWatchingOnly;
}

const QStringList& WalletPropertiesVM::seed() const
{
   return seed_;
}
