#include "OTCRequestViewModel.h"

OTCRequestViewModel::OTCRequestViewModel(QObject* parent)
   : QAbstractTableModel(parent)
{}

int OTCRequestViewModel::rowCount(const QModelIndex & parent) const
{
   if (parent.isValid()) {
      return 0;
   }

   return (int)inputs_.size();
}

int OTCRequestViewModel::columnCount(const QModelIndex & parent) const
{
   return ColumnCount;
}

void OTCRequestViewModel::clear()
{
   beginResetModel();
   inputs_.clear();
   endResetModel();
}

QVariant OTCRequestViewModel::data(const QModelIndex & index, int role) const
{
   switch (role) {
   case Qt::TextAlignmentRole:
      return int(Qt::AlignLeft | Qt::AlignVCenter);
   case Qt::DisplayRole:
      return getRowData(index.column(), inputs_[index.row()]);
   }
   return QVariant{};
}

QVariant OTCRequestViewModel::headerData(int section, Qt::Orientation orientation, int role) const
{
   if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
      switch (section) {
         case ColumnSecurity:
            return tr("Security");

         case ColumnType:
            return tr("Type");

         case ColumnProduct:
            return tr("Product");

         case ColumnSide:
            return tr("Side");

         case ColumnQuantity:
            return tr("Quantity");

         case ColumnDuration:
            return tr("Duration");
      }
   }

   return QVariant{};
}

QVariant OTCRequestViewModel::getRowData(const int column, const InputData& data) const
{
   switch(column) {
   case ColumnSecurity:
      return data.security;

   case ColumnType:
      return data.type;

   case ColumnProduct:
      return data.product;

   case ColumnSide:
      return data.side;

   case ColumnQuantity:
      return QVariant(data.quantity);

   case ColumnDuration:
      return QVariant(data.duration);
   }

   return QVariant{};
}

