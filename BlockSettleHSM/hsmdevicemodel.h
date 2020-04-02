/*

***********************************************************************************
* Copyright (C) 2016 - , BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef HSMDEVICEMODEL_H
#define HSMDEVICEMODEL_H

#include <QAbstractItemModel>
#include "hsmcommonstructure.h"

enum HSMDeviceRoles {
   DeviceId = Qt::UserRole + 1,
   Label,
   Vendor,
   PairedWallet,
   Status
};

class HSMDeviceModel : public QAbstractItemModel 
{
   Q_OBJECT
public:
   HSMDeviceModel(QObject *parent = nullptr);
   ~HSMDeviceModel() override = default;

   QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
   QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
   QModelIndex parent(const QModelIndex& index) const override;
   int rowCount(const QModelIndex& parent = QModelIndex()) const override;
   int columnCount(const QModelIndex& parent = QModelIndex()) const override;

   QHash<int, QByteArray> roleNames() const override;

   void resetModel(QVector<DeviceKey>&& deviceKey);
   DeviceKey getDevice(int index);

private:
   QVector<DeviceKey> devices_;
};

//Q_DECLARE_METATYPE(HSMDeviceModel*)

#endif // HSMDEVICEMODEL_H
