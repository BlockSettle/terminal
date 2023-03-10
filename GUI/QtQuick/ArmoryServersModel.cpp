/*

***********************************************************************************
* Copyright (C) 2023, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "ArmoryServersModel.h"
#include "ArmoryServersProvider.h"

#include <spdlog/spdlog.h>

namespace {
   static const QHash<int, QByteArray> kRoleNames{
      {ArmoryServersModel::TableDataRole, "tableData"},
      {ArmoryServersModel::NameRole, "name"},
      {ArmoryServersModel::NetTypeRole, "netType"},
      {ArmoryServersModel::AddressRole, "address"},
      {ArmoryServersModel::PortRole, "port"},
      {ArmoryServersModel::KeyRole, "key"},
      {ArmoryServersModel::DefaultServerRole, "isDefault"},
      {ArmoryServersModel::CurrentServerRole, "isCurrent"},
   };
}

ArmoryServersModel::ArmoryServersModel(const std::shared_ptr<spdlog::logger> & logger, QObject* parent)
   : QAbstractTableModel(parent)
   , header_{ tr("Name"), tr("Network"), tr("Address"), tr("Port"), tr("Key") }
   , logger_(logger)
{}

void ArmoryServersModel::setCurrent (int value)
{
   current_ = value;
   emit currentChanged();
}

void ArmoryServersModel::setData(int curIdx, int connIdx
   , const std::vector<ArmoryServer>& data)
{
   QMetaObject::invokeMethod(this, [this, curIdx, connIdx, data] {
      beginResetModel();
      data_ = data;
      endResetModel();

      if (current_ != curIdx) {
         setCurrent(curIdx);
      }
      if (connected_ != connIdx) {
         connected_ = connIdx;
         emit connectedChanged();
      }
      });
}

void ArmoryServersModel::add(const ArmoryServer& srv)
{
   QMetaObject::invokeMethod(this, [this, srv] {
      beginInsertRows(QModelIndex{}, rowCount(), rowCount());
      data_.push_back(srv);
      endInsertRows();
      });
}

bool ArmoryServersModel::del(int idx)
{
   if ((idx >= rowCount()) || (idx < 0) || (idx < ArmoryServersProvider::kDefaultServersCount)) {
      return false;
   }
   QMetaObject::invokeMethod(this, [this, idx] {
      beginRemoveRows(QModelIndex{}, idx, idx);
      data_.erase(data_.cbegin() + idx);
      endRemoveRows();
      });
   return true;
}

int ArmoryServersModel::rowCount(const QModelIndex&) const
{
   return data_.size();
}

int ArmoryServersModel::columnCount(const QModelIndex&) const
{
   return header_.size();
}

QVariant ArmoryServersModel::getData(int row, int col) const
{
   if ((row < 0) || (row >= rowCount())) {
      return {};
   }
   switch (col) {
   case 0:  return data_.at(row).name;
   case 1:  return (int)data_.at(row).netType;
   case 2:  return data_.at(row).armoryDBIp;
   case 3:  return QString::number(data_.at(row).armoryDBPort);
   case 4:  return data_.at(row).armoryDBKey;
   default: break;
   }
   return {};
}

QVariant ArmoryServersModel::data(const QModelIndex& index, int role) const
{
   switch (role) {
   case NameRole:          return getData(index.row(), 0);
   case NetTypeRole:       return getData(index.row(), 1);
   case AddressRole:       return getData(index.row(), 2);
   case PortRole:          return getData(index.row(), 3);
   case KeyRole:           return getData(index.row(), 4);
   case DefaultServerRole: return (index.row() < ArmoryServersProvider::kDefaultServersCount) && (index.row() < rowCount());
   case CurrentServerRole: return (index.row() == current());
   default: return getData(index.row(), index.column());
   }
   return {};
}

bool ArmoryServersModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
   if(!index.isValid() || index.row() > rowCount() || index.column() > columnCount()) {
      return false;
   }

   if (role == CurrentServerRole) {
      logger_->debug("[{}] current = {}", __func__, value.toInt());
      setCurrent(value.toInt());
   }
   else if (!isEditable(index.row())) {
      auto row = index.row();
      switch (role)
      {
      case NameRole:
         data_.at(row).name = value.toString();
      break;
      case NetTypeRole:
         data_.at(row).netType = static_cast<NetworkType>(value.toInt());
      break;
      case AddressRole:
         data_.at(row).armoryDBIp = value.toString();
      break;
      case PortRole:
         data_.at(row).armoryDBPort = value.toInt();
      break;
      case KeyRole:
         data_.at(row).armoryDBKey = value.toString();
      break;
      default:
      break;
      }
   }

   emit changed(index, value);
   return true;
}

QHash<int, QByteArray> ArmoryServersModel::roleNames() const
{
   return kRoleNames;
}

QVariant ArmoryServersModel::headerData(int section, Qt::Orientation orientation, int role) const
{
   if (orientation == Qt::Orientation::Horizontal) {
      return header_.at(section);
   }
   return {};
}

bool ArmoryServersModel::isEditable(int row) const
{
   return data(index(row,0), DefaultServerRole).toBool();
}
