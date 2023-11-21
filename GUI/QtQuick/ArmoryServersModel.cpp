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
      {Qt::DisplayRole, "display"},
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
   : QAbstractListModel(parent)
   , logger_(logger)
{
   connect(this, &ArmoryServersModel::modelReset,
           this, &ArmoryServersModel::rowCountChanged);
   connect(this, &ArmoryServersModel::rowsInserted,
           this, &ArmoryServersModel::rowCountChanged);
   connect(this, &ArmoryServersModel::rowsRemoved,
           this, &ArmoryServersModel::rowCountChanged);
}

void ArmoryServersModel::setCurrent (int value)
{
   logger_->debug("[{}] {} -> {}", __func__, current_, value);
   if (current_ == value) {
      return;
   }
   current_ = value;
   emit currentChanged(value);
   const auto dataIndex = index(current_, 0);
   emit dataChanged(dataIndex, dataIndex, { ArmoryServersModel::TableRoles::CurrentServerRole });
}

void ArmoryServersModel::setData(int curIdx, int connIdx
   , const std::vector<ArmoryServer>& data)
{
   QMetaObject::invokeMethod(this, [this, curIdx, connIdx, data] {
      beginResetModel();
      data_ = data;
      endResetModel();
      int newCur = curIdx;
      if (curIdx == -1) {
         newCur = 0;
      }
      setCurrent(newCur);
      if (connected_ != connIdx) {
         connected_ = connIdx;
         emit connectedChanged();
      }
   });
}

void ArmoryServersModel::add(const ArmoryServer& srv)
{
   beginInsertRows(QModelIndex{}, rowCount(), rowCount());
   data_.push_back(srv);
   endInsertRows();
}

// netType==0 => MainNet, netType==1 => TestNet
void ArmoryServersModel::add(QString name, QString armoryDBIp, int armoryDBPort, int netType, QString armoryDBKey)
{
   ArmoryServer server;
   server.name = name.toStdString();
   server.armoryDBPort = std::to_string(armoryDBPort);
   server.armoryDBIp = armoryDBIp.toStdString();
   server.armoryDBKey = armoryDBKey.toStdString();
   if (netType == 0) {
      server.netType = NetworkType::MainNet;
   }
   else if (netType == 1) {
      server.netType = NetworkType::TestNet;
   }

   QMetaObject::invokeMethod(this, [this, server] {
      add(server);
      setCurrent(rowCount() - 1);
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
   if (idx == current()) {
      //@dvajdual dont sure - what server must be default?
      setCurrent(0);
   }
   return true;
}

int ArmoryServersModel::rowCount(const QModelIndex&) const
{
   return data_.size();
}

QVariant ArmoryServersModel::data(const QModelIndex& index, int role) const
{
   if(!index.isValid() || index.row() > rowCount()) {
      return QVariant();
   }

   int row = index.row();

   switch (role) {
   case Qt::DisplayRole:   return (data_.at(row).netType == NetworkType::MainNet)
      ? (QString::fromStdString(data_.at(row).name) + QLatin1String(" (Mainnet)"))
      : (QString::fromStdString(data_.at(row).name) + QLatin1String(" (Testnet)"));
   case NameRole:          return QString::fromStdString(data_.at(row).name);
   case NetTypeRole:       return (int)data_.at(row).netType;
   case AddressRole:       return QString::fromStdString(data_.at(row).armoryDBIp);
   case PortRole:          return QString::fromStdString(data_.at(row).armoryDBPort);
   case KeyRole:           return QString::fromStdString(data_.at(row).armoryDBKey);
   case DefaultServerRole: return (index.row() < ArmoryServersProvider::kDefaultServersCount) && (index.row() < rowCount());
   case CurrentServerRole: return (index.row() == current());
   default: return QVariant();
   }
   return QVariant();
}

bool ArmoryServersModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
   if (!index.isValid() || index.row() > rowCount()) {
      return false;
   }

   int row = index.row();
   if (role == CurrentServerRole) {
      setCurrent(row);
   }
   else if (isEditable(index.row())) {
      switch (role)
      {
      case NameRole:
         data_.at(row).name = value.toString().toStdString();
         break;
      case NetTypeRole:
         data_.at(row).netType = static_cast<NetworkType>(value.toInt());
         break;
      case AddressRole:
         data_.at(row).armoryDBIp = value.toString().toStdString();
         break;
      case PortRole:
         data_.at(row).armoryDBPort = value.toString().toStdString();
         break;
      case KeyRole:
         data_.at(row).armoryDBKey = value.toString().toStdString();
         break;
      default: break;
      }
   }
   emit changed(index.row());
   emit dataChanged(index, index, { role });
   return true;
}

QHash<int, QByteArray> ArmoryServersModel::roleNames() const
{
   return kRoleNames;
}

bool ArmoryServersModel::isEditable(int row) const
{
   return !data(index(row), DefaultServerRole).toBool();
}

QString ArmoryServersModel::currentNetworkName() const
{
   if (current_ >= 0 && current_ < data_.size()) {
      return  QString::fromStdString(data_.at(current_).name);
   }
   return QString::fromLatin1("");
}
