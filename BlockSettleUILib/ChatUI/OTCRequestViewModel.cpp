#include "OTCRequestViewModel.h"

#include "OtcClient.h"
#include "OtcTypes.h"

using namespace bs::network;

namespace {

   const int kUpdateTimerInterval = 500;

   QString duration(QDateTime timestamp)
   {
      QDateTime endTimeStamp = timestamp.addSecs(
         std::chrono::duration_cast<std::chrono::seconds>(
            bs::network::otc::publicRequestTimeout()).count());
      const int oneMinuteInSec = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::minutes(1)).count();
      // We showing in one minute more since it's countdown
      const int timeLeftMinute = static_cast<int>(QDateTime::currentDateTime().secsTo(endTimeStamp) / oneMinuteInSec) 
         + std::chrono::minutes(1).count();
      const int minutes = std::min(10, static_cast<int>(timeLeftMinute));

      return QObject::tr("%1 min").arg(minutes);
   }

   QString side(bs::network::otc::Side requestSide, bool isOwnRequest) {
      if (!isOwnRequest) {
         requestSide = bs::network::otc::switchSide(requestSide);
      }

      return QString::fromStdString(otc::toString(requestSide));
   }

} // namespace

OTCRequestViewModel::OTCRequestViewModel(OtcClient *otcClient, QObject* parent)
   : QAbstractTableModel(parent)
   , otcClient_(otcClient)
{
   connect(&updateDurationTimer_, &QTimer::timeout, this, &OTCRequestViewModel::onUpdateDuration);

   updateDurationTimer_.setInterval(kUpdateTimerInterval);
   updateDurationTimer_.start();
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
            case Columns::Side:        return side(request.ourSide, requestData.isOwnRequest_);
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

QModelIndex OTCRequestViewModel::getIndexByTimestamp(QDateTime timeStamp)
{
   for (int iReq = 0; iReq < request_.size(); ++iReq) {
      if (timeStamp == request_[iReq].request_.timestamp) {
         return index(iReq, 0);
      }
   }

   return {};
}

void OTCRequestViewModel::onRequestsUpdated()
{
   beginResetModel();
   request_.clear();
   for (const auto &peer : otcClient_->requests()) {
      request_.push_back({ peer->request, peer->isOwnRequest });
   }
   endResetModel();
   emit restoreSelectedIndex();
}

void OTCRequestViewModel::onUpdateDuration()
{
   if (rowCount() == 0) {
      return;
   }

   emit dataChanged(index(0, static_cast<int>(Columns::Duration)),
      index(rowCount() - 1, static_cast<int>(Columns::Duration)), { Qt::DisplayRole });
}
