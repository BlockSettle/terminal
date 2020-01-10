/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __USED_INPITS_MODEL_H__
#define __USED_INPITS_MODEL_H__

#include <QAbstractTableModel>
#include <QString>

#include <vector>

struct UTXO;

class UsedInputsModel : public QAbstractTableModel
{
Q_OBJECT

public:

   UsedInputsModel(QObject* parent);
   ~UsedInputsModel() noexcept override = default;

   int rowCount(const QModelIndex & parent) const override;
   int columnCount(const QModelIndex & parent) const override;
   QVariant data(const QModelIndex & index, int role = Qt::DisplayRole) const override;
   QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

   void updateInputs(const std::vector<UTXO>& usedInputs);
   void clear();

private:
   enum Columns
   {
      ColumnAddress = 0,
      ColumnTxCount,
      ColumnBalance,
      ColumnCount
   };

   struct InputData
   {
      QString           address;
      unsigned int      txCount;
      double             balance;
   };
private:
   QVariant getRowData(const int column, const InputData& data) const;

private:
   std::vector<InputData> inputs_;
};


#endif // __USED_INPITS_MODEL_H__
