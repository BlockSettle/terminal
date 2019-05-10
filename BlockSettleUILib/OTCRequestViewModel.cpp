#include "OTCRequestViewModel.h"

OTCRequestViewModel::OTCRequestViewModel(QObject* parent)
   : QAbstractTableModel(parent)
{
   refreshTicker_.setInterval(500);
   connect(&refreshTicker_, &QTimer::timeout, this, &OTCRequestViewModel::RefreshBoard);
   refreshTicker_.start();
}

int OTCRequestViewModel::rowCount(const QModelIndex & parent) const
{
   if (parent.isValid()) {
      return 0;
   }

   return (int)currentRequests_.size();
}

int OTCRequestViewModel::columnCount(const QModelIndex &) const
{
   return ColumnCount;
}

void OTCRequestViewModel::clear()
{
   beginResetModel();
   currentRequests_.clear();
   endResetModel();
}

bs::network::LiveOTCRequest OTCRequestViewModel::GetOTCRequest(const QModelIndex& index)
{
   if (!index.isValid() || index.row() >= currentRequests_.size()) {
      return {};
   }

   return currentRequests_[index.row()];
}


QVariant OTCRequestViewModel::data(const QModelIndex & index, int role) const
{
   if (!index.isValid() || index.row() >= currentRequests_.size()) {
      return {};
   }

   switch (role) {
   case Qt::TextAlignmentRole:
      return int(Qt::AlignLeft | Qt::AlignVCenter);
   case Qt::DisplayRole:
      return getRowData(index.column(), currentRequests_[index.row()]);
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

QVariant OTCRequestViewModel::getRowData(const int column, const bs::network::LiveOTCRequest& otc) const
{
   switch(column) {
   case ColumnSecurity:
      return QLatin1String("EUR/XBT");

   case ColumnType:
      return QLatin1String("OTC");

   case ColumnProduct:
      return QLatin1String("XBT");

   case ColumnSide:
      return QString::fromStdString(bs::network::Side::toString(otc.side));

   case ColumnQuantity:
      return QString::fromStdString(bs::network::OTCRangeID::toString(otc.amountRange));

   case ColumnDuration:
      {
         auto currentTimestamp = QDateTime::currentDateTime().toMSecsSinceEpoch();
         if (currentTimestamp >= otc.expireTimestamp) {
            return 0;
         }

         return static_cast<int>((otc.expireTimestamp - currentTimestamp)/60000);
      }
   }

   return QVariant{};
}

void OTCRequestViewModel::AddLiveOTCRequest(const bs::network::LiveOTCRequest& otc)
{
   beginInsertRows(QModelIndex{}, currentRequests_.size(), currentRequests_.size());

   currentRequests_.emplace_back(otc);

   endInsertRows();
}

bool OTCRequestViewModel::RemoveOTCByID(const std::string& otc)
{
   // XXX simple solution. Not sure at what number of OTC requests this will start to slow down UI
   // will move to internal pointers and maps a bit later
   for (int i=0; i < currentRequests_.size(); ++i) {
      if (currentRequests_[i].otcId == otc) {
         beginRemoveRows(QModelIndex{}, i, i);
         currentRequests_.erase(currentRequests_.begin() + i);
         endRemoveRows();

         return true;
      }
   }

   return false;
}

void OTCRequestViewModel::RefreshBoard()
{
   if (!currentRequests_.empty()) {
      emit dataChanged(createIndex(0, static_cast<int>(ColumnDuration))
         , createIndex(currentRequests_.size()-1, static_cast<int>(ColumnDuration)), {Qt::DisplayRole});
   }
}
