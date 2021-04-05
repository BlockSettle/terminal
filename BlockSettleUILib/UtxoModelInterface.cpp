/*

***********************************************************************************
* Copyright (C) 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "UtxoModelInterface.h"
#include <QColor>

UtxoModelInterface::UtxoModelInterface(QObject* parent)
   :QAbstractTableModel(parent)
{
}

void UtxoModelInterface::enableRows(bool flag /*= true*/)
{
   if (rowsEnabled_ != flag) {
      rowsEnabled_ = flag;
      emit dataChanged(index(0, 0), index(rowCount({}) - 1, columnCount({}) - 1));
   }
}

Qt::ItemFlags UtxoModelInterface::flags(const QModelIndex &index) const
{
   if (rowsEnabled_) {
      return QAbstractTableModel::flags(index);
   }
   return Qt::ItemNeverHasChildren;
}

QVariant UtxoModelInterface::data(const QModelIndex & index, int role /*= Qt::DisplayRole*/) const
{
   switch (role) {
   case Qt::TextColorRole:
      return rowsEnabled_ ? QVariant{} : QColor(Qt::gray);
   }
   return QVariant{};
}
