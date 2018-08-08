#include "TransactionOutputsModel.h"

#include "UiUtils.h"

TransactionOutputsModel::TransactionOutputsModel(QObject* parent)
   : QAbstractTableModel{parent}
{}

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

QVariant TransactionOutputsModel::data(const QModelIndex & index, int role) const
{
   switch (role) {
   case Qt::TextAlignmentRole:
      if (index.column() == ColumnAmount) {
         return Qt::AlignRight;
      }
      return Qt::AlignLeft;
   case Qt::DisplayRole:
      return getRowData(index.column(), outputs_[index.row()]);
   }
   return QVariant{};
}

void TransactionOutputsModel::AddRecipient(unsigned int recipientId, const QString& address, double amount)
{
   beginInsertRows(QModelIndex{}, (int)outputs_.size(), (int)outputs_.size());
   outputs_.emplace_back(OutputRow{recipientId, address, amount});
   endInsertRows();
}

void TransactionOutputsModel::RemoveRecipient(int row)
{
   beginRemoveRows(QModelIndex{}, row, row);

   outputs_.erase(outputs_.begin() + row);

   endRemoveRows();
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
   switch(column){
   case ColumnAddress:
      return outputRow.address;
   case ColumnAmount:
      return UiUtils::displayAmount(outputRow.amount);
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
