#include <QFont>
#include <QTreeView>
#include "ArmoryServersViewModel.h"
#include "EncryptionUtils.h"
#include "ArmoryConnection.h"

namespace {
   int kArmoryServerColumns = 5;
}

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
   return kArmoryServerColumns;
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

   if (role == Qt::FontRole && index.row() == currentServerIndex) {
       QFont font;
       font.setBold(true);
       return font;
   }
   else if (role == Qt::TextColorRole && index.row() == currentServerIndex && highLightSelectedServer_) {
       return QColor(Qt::white);
   }
   else if (role == Qt::DisplayRole) {
      switch (index.column()) {
      case 0:
         return server.name;
      case 1:
         return server.netType == NetworkType::MainNet ? tr("MainNet") : tr("TestNet");
      case 2:
         return server.armoryDBIp;
      case 3:
         return server.armoryDBPort;
      case 4:
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

void ArmoryServersViewModel::setHighLightSelectedServer(bool highLightSelectedServer)
{
   highLightSelectedServer_ = highLightSelectedServer;
}


