/*

***********************************************************************************
* Copyright (C) 2020 - 2021, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef HWDEVICEMODEL_H
#define HWDEVICEMODEL_H

#include <QAbstractItemModel>
#include "hwdeviceinterface.h"
#include "hwcommonstructure.h"

namespace spdlog {
   class logger;
}

enum HwDeviceRoles {
   DeviceId = Qt::UserRole + 1,
   Label,
   Vendor,
   PairedWallet,
   Status
};

class HwDeviceModel : public QAbstractItemModel 
{
   Q_OBJECT
   Q_PROPERTY(int selDevice READ selDevice NOTIFY selected)
   Q_PROPERTY(bool empty READ empty NOTIFY dataSet)
   bool empty() const { return devices_.empty(); }

public:
   HwDeviceModel(const std::shared_ptr<spdlog::logger>&, QObject *parent = nullptr);
   ~HwDeviceModel() override = default;

   QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
   QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
   QModelIndex parent(const QModelIndex& index) const override;
   int rowCount(const QModelIndex& parent = QModelIndex()) const override;
   int columnCount(const QModelIndex& parent = QModelIndex()) const override;

   QHash<int, QByteArray> roleNames() const override;

   void setDevices(const std::vector<bs::hww::DeviceKey>&);
   void setLoaded(const std::string& walletId);
   bs::hww::DeviceKey getDevice(int index);
   int getDeviceIndex(bs::hww::DeviceKey key);
   void findNewDevice();

signals:
   void selected();
   void dataSet();

private:
   int selDevice() const;

private:
   std::shared_ptr<spdlog::logger>  logger_;
   std::vector<bs::hww::DeviceKey>  devices_;
   std::vector<bool>                loaded_;
};

Q_DECLARE_METATYPE(HwDeviceModel*)

#endif // HWDEVICEMODEL_H
