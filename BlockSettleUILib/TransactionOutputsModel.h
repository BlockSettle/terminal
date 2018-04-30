#ifndef __TRANSACTION_OUTPUTS_MODEL_H__
#define __TRANSACTION_OUTPUTS_MODEL_H__

#include <QAbstractTableModel>

#include <vector>

class TransactionOutputsModel : public QAbstractTableModel
{
Q_OBJECT

public:
   TransactionOutputsModel(QObject* parent);
   ~TransactionOutputsModel() noexcept override = default;

   void AddRecipient(unsigned int recipientId, const QString& address, double amount);

   unsigned int   GetOutputId(int row);
   int            GetRowById(unsigned int id);
   void           RemoveRecipient(int row);

   void clear();
   int rowCount(const QModelIndex & parent) const override;
   int columnCount(const QModelIndex & parent) const override;
   QVariant data(const QModelIndex & index, int role = Qt::DisplayRole) const override;
   QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

private:
   struct OutputRow
   {
      unsigned int   recipientId;
      QString        address;
      double         amount;
   };

   enum Columns
   {
      ColumnAddress = 0,
      ColumnAmount,
      ColumnRemove,
      ColumnCount
   };

private:
   QVariant getRowData(int column, const OutputRow& outputRow) const;

private:
   std::vector<OutputRow> outputs_;
};

#endif // __TRANSACTION_OUTPUTS_MODEL_H__
