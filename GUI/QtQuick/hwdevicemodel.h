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
#include "hwcommonstructure.h"

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
   Q_PROPERTY(int toppestImport READ toppestImport NOTIFY toppestImportChanged)
public:
   HwDeviceModel(QObject *parent = nullptr);
   ~HwDeviceModel() override = default;

   QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
   QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
   QModelIndex parent(const QModelIndex& index) const override;
   int rowCount(const QModelIndex& parent = QModelIndex()) const override;
   int columnCount(const QModelIndex& parent = QModelIndex()) const override;

   QHash<int, QByteArray> roleNames() const override;

#ifdef BUILD_HW_WALLETS
   void resetModel(QVector<DeviceKey>&& deviceKey);
   DeviceKey getDevice(int index);
   int getDeviceIndex(DeviceKey key);
#endif

   Q_INVOKABLE int toppestImport() const;

signals:
   void toppestImportChanged();

private:
#ifdef BUILD_HW_WALLETS
   QVector<DeviceKey> devices_;
#endif
};

Q_DECLARE_METATYPE(HwDeviceModel*)

#endif // HWDEVICEMODEL_H
