/*

***********************************************************************************
* Copyright (C) 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "hwdevicemodel.h"

HwDeviceModel::HwDeviceModel(QObject *parent /*= nullptr*/)
   : QAbstractItemModel(parent)
{
}

QVariant HwDeviceModel::data(const QModelIndex& index, int role /*= Qt::DisplayRole*/) const
{
   if (!index.isValid()) {
      return {};
   }

   const int row = index.row();
   if (row < 0 || row > devices_.size()) {
      assert(false);
      return {};
   }

   switch (static_cast<HwDeviceRoles>(role))
   {
   case HwDeviceRoles::DeviceId:
      return devices_[row].deviceId_;
   case HwDeviceRoles::Label:
      return devices_[row].deviceLabel_;
   case HwDeviceRoles::Vendor:
      return devices_[row].vendor_;
   case HwDeviceRoles::PairedWallet:
         return devices_[row].walletId_;
   case HwDeviceRoles::Status:
      return devices_[row].status_;
   default:
      break;
   }

   return {};
}

QModelIndex HwDeviceModel::index(int row, int column, const QModelIndex& parent /*= QModelIndex()*/) const
{
   if (parent.isValid() && column != 0) {
      return {};
   }

   if (!hasIndex(row, column, parent)) {
      return {};
   }

   return createIndex(row, column);
}

QModelIndex HwDeviceModel::parent(const QModelIndex& index) const
{
   return {};
}

int HwDeviceModel::rowCount(const QModelIndex& parent /*= QModelIndex()*/) const
{
   return devices_.size();
}

int HwDeviceModel::columnCount(const QModelIndex& parent /*= QModelIndex()*/) const
{
   return 1;
}

void HwDeviceModel::resetModel(QVector<DeviceKey>&& deviceKeys)
{
   beginResetModel();
   devices_ = std::move(deviceKeys);
   endResetModel();
   emit toppestImportChanged();
}

DeviceKey HwDeviceModel::getDevice(int index)
{
   if (index < 0 || index > devices_.size()) {
      return {};
   }

   return devices_[index];
}

int HwDeviceModel::getDeviceIndex(DeviceKey key)
{
   for (int i = 0; i < devices_.size(); ++i) {
      if (devices_[i].deviceId_ == key.deviceId_) {
         return i;
      }
   }

   return -1;
}

int HwDeviceModel::toppestImport() const
{
   if (devices_.empty()) {
      return -1;
   }

   for (int i = 0; i < devices_.size(); ++i) {
      if (devices_[i].status_.isEmpty() && devices_[i].walletId_.isEmpty()) {
         return i;
      }
   }

   return -1;
}

QHash<int, QByteArray> HwDeviceModel::roleNames() const
{
   return {
      { HwDeviceRoles::DeviceId , "deviceId" },
      { HwDeviceRoles::Label , "label" },
      { HwDeviceRoles::Vendor , "vendor" },
      { HwDeviceRoles::PairedWallet , "pairedWallet" },
      { HwDeviceRoles::PairedWallet , "pairedWallet" },
      { HwDeviceRoles::Status , "status"}
   };
}
