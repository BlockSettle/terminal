/*

***********************************************************************************
* Copyright (C) 2018 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __TRANSACTION_OUTPUTS_MODEL_H__
#define __TRANSACTION_OUTPUTS_MODEL_H__

#include "UtxoModelInterface.h"
#include <tuple>
#include <vector>
#include "XBTAmount.h"

class TransactionOutputsModel : public UtxoModelInterface
{
Q_OBJECT

public:
   TransactionOutputsModel(QObject* parent);
   ~TransactionOutputsModel() noexcept override = default;

   void AddRecipient(unsigned int recipientId, const QString& address, const bs::XBTAmount &);
   void AddRecipients(const std::vector<std::tuple<unsigned int, QString, bs::XBTAmount>> &);
   void UpdateRecipientAmount(unsigned int recipientId, const bs::XBTAmount &);

   unsigned int   GetOutputId(int row);
   int            GetRowById(unsigned int id);
   void           RemoveRecipient(int row);
   bool           isRemoveColumn(int column);

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
      bs::XBTAmount  amount;
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
   QVariant getImageData(const int column) const;

private:
   std::vector<OutputRow> outputs_;
   QIcon removeIcon_;
};

#endif // __TRANSACTION_OUTPUTS_MODEL_H__
