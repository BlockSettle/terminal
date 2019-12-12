/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include <QFont>
#include <QTreeView>
#include "ArmoryServersViewModel.h"
#include "EncryptionUtils.h"
#include "ArmoryConnection.h"

ArmoryServersViewModel::ArmoryServersViewModel(const std::shared_ptr<ArmoryServersProvider> &serversProvider
                                               , QObject *parent)
   : QAbstractTableModel(parent)
   , serversProvider_(serversProvider)
{
   update();
   connect(serversProvider.get(), &ArmoryServersProvider::dataChanged, this, &ArmoryServersViewModel::update);
}

int ArmoryServersViewModel::columnCount(const QModelIndex&) const
{
   return static_cast<int>(ArmoryServersViewModel::ColumnsCount);
}

int ArmoryServersViewModel::rowCount(const QModelIndex&) const
{
   return servers_.size();
}

QVariant ArmoryServersViewModel::data(const QModelIndex &index, int role) const
{
   if (index.row() >= servers_.size()) return QVariant();
   ArmoryServer server = servers_.at(index.row());
   int currentServerIndex = serversProvider_->indexOfCurrent();
   QString serverNetType = (server.netType == NetworkType::MainNet ? tr("MainNet") : tr("TestNet"));

   if (role == Qt::FontRole && index.row() == currentServerIndex) {
//       QFont font;
//       font.setBold(true);
//       return font;
   }
   else if (role == Qt::TextColorRole && index.row() == currentServerIndex && highLightSelectedServer_) {
       return QColor(Qt::white);
   }
   else if (role == Qt::DisplayRole) {
      switch (index.column()) {
      case ColumnName:
         if (singleColumnMode_) {
            return QStringLiteral("%1 (%2)").arg(server.name).arg(serverNetType);
         }
         else {
            return server.name;
         }
      case ColumnType:
         return serverNetType;
      case ColumnAddress:
         return server.armoryDBIp;
      case ColumnPort:
         return server.armoryDBPort;
      case ColumnKey:
         return server.armoryDBKey;
      default:
         break;
      }
   }
   return QVariant();
}

QVariant ArmoryServersViewModel::headerData(int section, Qt::Orientation orientation, int role) const
{
   if (orientation != Qt::Horizontal) {
      return QVariant();
   }

   if (role == Qt::DisplayRole) {
      switch(static_cast<ArmoryServersViewViewColumns>(section)) {
      case ArmoryServersViewViewColumns::ColumnName:
         return tr("Name");
      case ArmoryServersViewViewColumns::ColumnType:
         return tr("Network");
      case ArmoryServersViewViewColumns::ColumnAddress:
         return tr("Address");
      case ArmoryServersViewViewColumns::ColumnPort:
         return tr("Port");
      case ArmoryServersViewViewColumns::ColumnKey:
         return tr("Key");
      default:
         return QVariant();
      }
   }

   return QVariant();
}

void ArmoryServersViewModel::update()
{
   beginResetModel();
   servers_ = serversProvider_->servers();
   endResetModel();
}

void ArmoryServersViewModel::setSingleColumnMode(bool singleColumnMode)
{
   singleColumnMode_ = singleColumnMode;
}

void ArmoryServersViewModel::setHighLightSelectedServer(bool highLightSelectedServer)
{
   highLightSelectedServer_ = highLightSelectedServer;
}


