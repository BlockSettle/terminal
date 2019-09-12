#ifndef __OTC_REQUEST_VIEW_MODEL_H__
#define __OTC_REQUEST_VIEW_MODEL_H__

#include <QAbstractTableModel>
#include <QString>
#include <QTimer>

#include <vector>

#include "CommonTypes.h"
#include "chat.pb.h"

class OTCRequestViewModel : public QAbstractTableModel
{
Q_OBJECT
public:
   OTCRequestViewModel(QObject* parent);
   ~OTCRequestViewModel() noexcept override = default;

   int rowCount(const QModelIndex & parent) const override;
   int columnCount(const QModelIndex & parent) const override;
   QVariant data(const QModelIndex & index, int role = Qt::DisplayRole) const override;
   QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

   void clear();

   void AddLiveOTCRequest(const std::shared_ptr<Chat::Data>& otc);
   bool RemoveOTCByID(const std::string &serverRequestId);

   std::shared_ptr<Chat::Data> GetOTCRequest(const QModelIndex& index);

private slots:
   // update time left only. do not remove anything
   void RefreshBoard();

private:
   enum Columns
   {
      ColumnSecurity,
      ColumnType,
      ColumnProduct,
      ColumnSide,
      ColumnQuantity,
      ColumnDuration,

      ColumnCount
   };

   QVariant getRowData(const int column, const std::shared_ptr<Chat::Data>& otc) const;

   std::vector<std::shared_ptr<Chat::Data>>  currentRequests_;
   QTimer                                    refreshTicker_;
};

#endif
