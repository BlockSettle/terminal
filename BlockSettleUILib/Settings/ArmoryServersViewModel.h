/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __ARMORY_SERVERS_VEIW_MODEL_H__
#define __ARMORY_SERVERS_VEIW_MODEL_H__

#include <QAbstractTableModel>
#include <memory>

#include "AuthAddress.h"
#include "AuthAddressManager.h"
#include "BinaryData.h"
#include "ApplicationSettings.h"
#include "ArmoryServersProvider.h"


class ArmoryServersViewModel : public QAbstractTableModel
{
public:
   ArmoryServersViewModel(const std::shared_ptr<ArmoryServersProvider>& serversProvider
                          , QObject *parent = nullptr);
   ~ArmoryServersViewModel() noexcept = default;

   ArmoryServersViewModel(const ArmoryServersViewModel&) = delete;
   ArmoryServersViewModel& operator = (const ArmoryServersViewModel&) = delete;

   ArmoryServersViewModel(ArmoryServersViewModel&&) = delete;
   ArmoryServersViewModel& operator = (ArmoryServersViewModel&&) = delete;

   int columnCount(const QModelIndex &parent = QModelIndex()) const override;
   int rowCount(const QModelIndex &parent = QModelIndex()) const override;

   QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
   QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

   void setHighLightSelectedServer(bool highLightSelectedServer);
   void setSingleColumnMode(bool singleColumnMode);

public slots:
   void update();

private:
   std::shared_ptr<ArmoryServersProvider> serversProvider_;
   QList<ArmoryServer> servers_;
   bool highLightSelectedServer_ = true;
   bool singleColumnMode_ = false;

   enum ArmoryServersViewViewColumns : int
   {
      ColumnName,
      ColumnType,
      ColumnAddress,
      ColumnPort,
      ColumnKey,
      ColumnsCount
   };
};

#endif // __ARMORY_SERVERS_VEIW_MODEL_H__
