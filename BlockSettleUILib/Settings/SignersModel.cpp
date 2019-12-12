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
#include "SignersModel.h"

SignersModel::SignersModel(const std::shared_ptr<SignersProvider>& signersProvider
                                               , QObject *parent)
   : QAbstractTableModel(parent)
   , signersProvider_(signersProvider)
{
   update();
   connect(signersProvider.get(), &SignersProvider::dataChanged, this, &SignersModel::update);
}

int SignersModel::columnCount(const QModelIndex&) const
{
   return static_cast<int>(SignersModel::ColumnsCount);
}

int SignersModel::rowCount(const QModelIndex&) const
{
   return signers_.size();
}

QVariant SignersModel::data(const QModelIndex &index, int role) const
{
   if (index.row() >= signers_.size()) {
      return QVariant();
   }

   SignerHost signerHost = signers_.at(index.row());

   int currentServerIndex = signersProvider_->indexOfCurrent();

   if (role == Qt::FontRole && index.row() == currentServerIndex) {
//       QFont font;
//       font.setBold(true);
//       return font;
   } else if (role == Qt::TextColorRole && index.row() == currentServerIndex && highLightSelectedServer_) {
       return QColor(Qt::white);
   } else if (role == Qt::DisplayRole) {
      switch (index.column()) {
      case ColumnName:
         return signerHost.name;
      case ColumnAddress:
         return signerHost.address;
      case ColumnPort:
         return signerHost.port != 0 ? QString::number(signerHost.port) : tr("Auto");
      case ColumnKey:
         return signerHost.key;
      default:
         break;
      }
   }
   return QVariant();
}

QVariant SignersModel::headerData(int section, Qt::Orientation orientation, int role) const
{
   if (orientation != Qt::Horizontal) {
      return QVariant();
   }

   if (role == Qt::DisplayRole) {
      switch(static_cast<SignersViewColumns>(section)) {
      case SignersViewColumns::ColumnName:
         return tr("Name");
      case SignersViewColumns::ColumnAddress:
         return tr("Address");
      case SignersViewColumns::ColumnPort:
         return tr("Port");
      case SignersViewColumns::ColumnKey:
         return tr("Key");
      default:
         return QVariant();
      }
   }

   return QVariant();
}

void SignersModel::update()
{
   beginResetModel();
   signers_ = signersProvider_->signers();
   endResetModel();
}

void SignersModel::setSingleColumnMode(bool singleColumnMode)
{
   singleColumnMode_ = singleColumnMode;
}

void SignersModel::setHighLightSelectedServer(bool highLightSelectedServer)
{
   highLightSelectedServer_ = highLightSelectedServer;
}
