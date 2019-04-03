#ifndef __OTC_REQUEST_VIEW_MODEL_H__
#define __OTC_REQUEST_VIEW_MODEL_H__

#include <QAbstractTableModel>
#include <QString>

#include <vector>

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

private:
   enum Columns
   {
      ColumnSecurity = 0,
      ColumnType,
      ColumnProduct,
      ColumnSide,
      ColumnQuantity,
      ColumnDuration,
      ColumnCount
   };

   struct InputData
   {
      QString           security;
      QString           type;
      QString           product;
      QString           side;
      unsigned int      quantity;
      unsigned int      duration;
   };
private:
   QVariant getRowData(const int column, const InputData& data) const;

private:
   std::vector<InputData> inputs_;
};

#endif