/*

***********************************************************************************
* Copyright (C) 2016 - , BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "hsmdevicemodel.h"

HSMDeviceModel::HSMDeviceModel(QObject *parent /*= nullptr*/)
   : QAbstractItemModel(parent)
{
}

QVariant HSMDeviceModel::data(const QModelIndex& index, int role /*= Qt::DisplayRole*/) const
{
   if (!index.isValid()) {
      return {};
   }

   const int row = index.row();
   if (row < 0 || row > devices_.size()) {
      assert(false);
      return {};
   }

   switch (static_cast<HSMDeviceRoles>(role))
   {
   case HSMDeviceRoles::DeviceId:
      return devices_[row].deviceId_;
   case HSMDeviceRoles::Label:
      return devices_[row].deviceLabel_;
   case HSMDeviceRoles::Vendor:
      return devices_[row].vendor_;
   case HSMDeviceRoles::PairedWallet:
         return devices_[row].walletId_;
   default:
      break;
   }

   return {};
}

QModelIndex HSMDeviceModel::index(int row, int column, const QModelIndex& parent /*= QModelIndex()*/) const
{
   if (parent.isValid() && column != 0) {
      return {};
   }

   if (!hasIndex(row, column, parent)) {
      return {};
   }

   return createIndex(row, column);
}

QModelIndex HSMDeviceModel::parent(const QModelIndex& index) const
{
   return {};
}

int HSMDeviceModel::rowCount(const QModelIndex& parent /*= QModelIndex()*/) const
{
   return devices_.size();
}

int HSMDeviceModel::columnCount(const QModelIndex& parent /*= QModelIndex()*/) const
{
   return 1;
}

void HSMDeviceModel::resetModel(QVector<DeviceKey>&& deviceKeys)
{
   beginResetModel();
   devices_ = std::move(deviceKeys);
   endResetModel();
}

DeviceKey HSMDeviceModel::getDevice(int index)
{
   if (index < 0 || index > devices_.size()) {
      return {};
   }

   return devices_[index];
}

QHash<int, QByteArray> HSMDeviceModel::roleNames() const
{
   return {
      { HSMDeviceRoles::DeviceId , "deviceId" },
      { HSMDeviceRoles::Label , "label" },
      { HSMDeviceRoles::Vendor , "vendor" },
      { HSMDeviceRoles::PairedWallet , "pairedWallet" }
   };
}
