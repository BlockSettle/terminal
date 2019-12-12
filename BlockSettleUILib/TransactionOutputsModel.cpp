/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include <QColor>
#include <QSize>
#include <QIcon>
#include "TransactionOutputsModel.h"
#include "UiUtils.h"

TransactionOutputsModel::TransactionOutputsModel(QObject* parent)
   : QAbstractTableModel{parent}
{
   removeIcon_ = UiUtils::icon(0xeaf1, QVariantMap{
               { QLatin1String{ "color" }, QColor{ Qt::white } }
            });
}

int TransactionOutputsModel::rowCount(const QModelIndex & parent) const
{
   if (parent.isValid()) {
      return 0;
   }

   return (int)outputs_.size();
}

int TransactionOutputsModel::columnCount(const QModelIndex & parent) const
{
   return ColumnCount;
}

void TransactionOutputsModel::clear()
{
   beginResetModel();
   outputs_.clear();
   endResetModel();
}

void TransactionOutputsModel::enableRows(bool flag)
{
   if (rowsEnabled_ != flag) {
      rowsEnabled_ = flag;
      emit dataChanged(index(0, 0), index(rowCount({}) - 1, columnCount({}) - 1));
   }
}

Qt::ItemFlags TransactionOutputsModel::flags(const QModelIndex &index) const
{
   if (rowsEnabled_) {
      return QAbstractTableModel::flags(index);
   }
   return Qt::ItemNeverHasChildren;
}

QVariant TransactionOutputsModel::data(const QModelIndex & index, int role) const
{
   // workaround dont working here
   // TODO:move "Delete output button"
   // from CreateTransactionDialogAdvanced::onOutputsInserted to model delegate
  if (role == Qt::SizeHintRole && index.column() == 2) {
     return QSize(50, 14);
  }

   switch (role) {
   case Qt::TextAlignmentRole:
      return int (Qt::AlignLeft | Qt::AlignVCenter);
   case Qt::DisplayRole:
      return getRowData(index.column(), outputs_[index.row()]);
   case Qt::DecorationRole:
      return getImageData(index.column());
   case Qt::TextColorRole:
      return rowsEnabled_ ? QVariant{} : QColor(Qt::gray);
   }
   return QVariant{};
}

void TransactionOutputsModel::AddRecipient(unsigned int recipientId, const QString& address, double amount)
{
   beginInsertRows(QModelIndex{}, (int)outputs_.size(), (int)outputs_.size());
   outputs_.emplace_back(OutputRow{recipientId, address, amount});
   endInsertRows();
}

void TransactionOutputsModel::AddRecipients(const std::vector<std::tuple<unsigned int, QString, double>> &recipients)
{
   beginInsertRows(QModelIndex{}, (int)outputs_.size(), (int)outputs_.size());
   for (const auto &recip : recipients) {
      outputs_.emplace_back(OutputRow{ std::get<0>(recip), std::get<1>(recip), std::get<2>(recip) });
   }
   endInsertRows();
}

void TransactionOutputsModel::UpdateRecipientAmount(unsigned int recipientId, double amount)
{
   int row = -1;
   for (int i = 0; i < outputs_.size(); ++i) {
      if (outputs_[i].recipientId == recipientId) {
         row = i;
         outputs_[i].amount = amount;
         break;
      }
   }
   emit dataChanged(index(row, ColumnAmount), index(row, ColumnAmount), { Qt::DisplayRole });
}

void TransactionOutputsModel::RemoveRecipient(int row)
{
   beginRemoveRows(QModelIndex{}, row, row);

   outputs_.erase(outputs_.begin() + row);

   endRemoveRows();
}

bool TransactionOutputsModel::isRemoveColumn(int column)
{
   return column == ColumnRemove;
}

unsigned int TransactionOutputsModel::GetOutputId(int row)
{
   return outputs_[row].recipientId;
}

int TransactionOutputsModel::GetRowById(unsigned int id)
{
   for (int i=0; i < outputs_.size(); i++) {
      if (outputs_[i].recipientId == id) {
         return i;
      }
   }
   return -1;
}

QVariant TransactionOutputsModel::getRowData(int column, const OutputRow& outputRow) const
{
   switch (column) {
   case ColumnAddress:
      return outputRow.address;
   case ColumnAmount:
      return UiUtils::displayAmount(outputRow.amount);
   }

   return QVariant{};
}

QVariant TransactionOutputsModel::getImageData(const int column) const
{
   if (column == ColumnRemove && rowsEnabled_) {
      return removeIcon_;
   }

   return QVariant{};  
}

QVariant TransactionOutputsModel::headerData(int section, Qt::Orientation orientation, int role) const
{
   if (orientation == Qt::Horizontal)
   {
      switch (role) {
      case Qt::DisplayRole:
         if (section == ColumnAddress) {
            return tr("Address");
         }
         else if (section == ColumnAmount) {
            return tr("Amount");
         }
         return QString();
      case Qt::TextAlignmentRole:
         if (section == ColumnAddress){
            return static_cast<int>(Qt::AlignLeft | Qt::AlignVCenter);
         }
         return static_cast<int>(Qt::AlignRight | Qt::AlignVCenter);
      }
   }

   return QVariant{};
}
