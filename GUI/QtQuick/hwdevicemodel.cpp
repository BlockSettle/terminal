/*

***********************************************************************************
* Copyright (C) 2020 - 2021, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "hwdevicemodel.h"

HwDeviceModel::HwDeviceModel(QObject *parent /*= nullptr*/)
   : QAbstractItemModel(parent)
{}

QVariant HwDeviceModel::data(const QModelIndex& index, int role /*= Qt::DisplayRole*/) const
{
   if (!index.isValid()) {
      return {};
   }
   const int row = index.row();
   if (row < 0 || row >= devices_.size()) {
      assert(false);
      return {};
   }

   switch (static_cast<HwDeviceRoles>(role))
   {
   case HwDeviceRoles::DeviceId:
      return QString::fromStdString(devices_.at(row).id);
   case HwDeviceRoles::Label:
      return QString::fromStdString(devices_.at(row).label);
   case HwDeviceRoles::Vendor:
      return QString::fromStdString(devices_.at(row).vendor);
   case HwDeviceRoles::PairedWallet:
         return QString::fromStdString(devices_.at(row).walletId);
   case HwDeviceRoles::Status:
      return QString::fromStdString(devices_.at(row).status);
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

int HwDeviceModel::rowCount(const QModelIndex&) const
{
   return devices_.size();
}

int HwDeviceModel::columnCount(const QModelIndex&) const
{
   return 1;
}

void HwDeviceModel::setDevices(const std::vector<bs::hww::DeviceKey>& deviceKeys)
{
   beginResetModel();
   loaded_.clear();
   loaded_.resize(deviceKeys.size(), false);
   devices_ = deviceKeys;
   endResetModel();
   emit dataSet();
}

bs::hww::DeviceKey HwDeviceModel::getDevice(int index)
{
   if (index < 0 || index >= devices_.size()) {
      return {};
   }
   return devices_.at(index);
}

int HwDeviceModel::getDeviceIndex(bs::hww::DeviceKey key)
{
   for (int i = 0; i < devices_.size(); ++i) {
      if (devices_.at(i).id == key.id) {
         return i;
      }
   }
   return -1;
}

void HwDeviceModel::setLoaded(const std::string& walletId)
{
   for (int i = 0; i < devices_.size(); ++i) {
      const auto& device = devices_.at(i);
      if (device.walletId == walletId) {
         loaded_[i] = true;
      }
   }
   emit dataSet();
}

int HwDeviceModel::selDevice() const
{
   for (int i = 0; i < loaded_.size(); ++i) {
      if (!loaded_.at(i)) {
         return i;
      }
   }
   return -1;
}

void HwDeviceModel::findNewDevice()
{
   if (selDevice() >= 0) {
      emit selected();
   }
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
