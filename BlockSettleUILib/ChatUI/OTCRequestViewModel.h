#ifndef __OTC_REQUEST_VIEW_MODEL_H__
#define __OTC_REQUEST_VIEW_MODEL_H__

#include <QAbstractTableModel>

#include "OtcTypes.h"

class OtcClient;

class OTCRequestViewModel : public QAbstractTableModel
{
   Q_OBJECT

public:
   OTCRequestViewModel(OtcClient *otcClient, QObject* parent = nullptr);
   ~OTCRequestViewModel() override = default;

   int rowCount(const QModelIndex &parent) const override;
   int columnCount(const QModelIndex &parent) const override;
   QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
   QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

private slots:
   void onRequestsUpdated();

private:
   enum class Columns
   {
      Security,
      Type,
      Product,
      Side,
      Quantity,
      Duration,

      Latest = Duration,
   };

   std::vector<bs::network::otc::QuoteRequest> request_;

   OtcClient *otcClient_{};

};

#endif
