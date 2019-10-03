#include "OTCRequestViewModel.h"

#include "OtcClient.h"

using namespace bs::network;

namespace {

   const int kMaxPeriodMinutes = 10;

   QString duration(QDateTime timestamp)
   {
      int minutes = std::max(0, int(QDateTime::currentDateTime().secsTo(timestamp) / 60));
      if (minutes > kMaxPeriodMinutes) {
         return QObject::tr("> %1 min").arg(kMaxPeriodMinutes);
      }
      return QObject::tr("%1 min").arg(minutes);
   }

} // namespace

OTCRequestViewModel::OTCRequestViewModel(OtcClient *otcClient, QObject* parent)
   : QAbstractTableModel(parent)
   , otcClient_(otcClient)
{
   connect(otcClient, &OtcClient::publicUpdated, this, &OTCRequestViewModel::onRequestsUpdated);
}

int OTCRequestViewModel::rowCount(const QModelIndex &parent) const
{
   return int(request_.size());
}

int OTCRequestViewModel::columnCount(const QModelIndex &parent) const
{
   return int(Columns::Latest) + 1;
}

QVariant OTCRequestViewModel::data(const QModelIndex &index, int role) const
{
   const auto &requestData = request_.at(size_t(index.row()));
   const auto &request = requestData.request_;
   const auto column = Columns(index.column());

   switch (role) {
      case Qt::TextAlignmentRole:
         return { static_cast<int>(Qt::AlignLeft | Qt::AlignVCenter) };

      case Qt::DisplayRole:
         switch (column) {
            case Columns::Security:    return QStringLiteral("EUR/XBT");
            case Columns::Type:        return QStringLiteral("OTC");
            case Columns::Product:     return QStringLiteral("XBT");
            case Columns::Side:        return QString::fromStdString(otc::toString(request.ourSide));
            case Columns::Quantity:    return QString::fromStdString(otc::toString(request.rangeType));
            case Columns::Duration:    return duration(request.timestamp);
         }
         assert(false);
         return {};

      case static_cast<int>(CustomRoles::OwnQuote):
         return { requestData.isOwnRequest_ };

      default:
         return {};
   }
}

QVariant OTCRequestViewModel::headerData(int section, Qt::Orientation orientation, int role) const
{
   if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
      switch (Columns(section)) {
         case Columns::Security:       return tr("Security");
         case Columns::Type:           return tr("Type");
         case Columns::Product:        return tr("Product");
         case Columns::Side:           return tr("Side");
         case Columns::Quantity:       return tr("Quantity");
         case Columns::Duration:       return tr("Duration");
      }
      assert(false);
      return {};
   }

   return QVariant{};
}

void OTCRequestViewModel::onRequestsUpdated()
{
   beginResetModel();
   request_.clear();
   for (const auto &peer : otcClient_->requests()) {
      request_.push_back({ peer->request, peer->isOwnRequest });
   }
   endResetModel();
}
