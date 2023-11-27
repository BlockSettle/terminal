/*

***********************************************************************************
* Copyright (C) 2023, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef TX_INPUTS_SELECTED_MODEL_H
#define TX_INPUTS_SELECTED_MODEL_H

#include "TxInputsModel.h"

class TxInputsSelectedModel : public QAbstractTableModel
{
   Q_OBJECT
   Q_PROPERTY(int rowCount READ rowCount NOTIFY rowCountChanged)

public:
   enum TableRoles {
      TableDataRole = Qt::UserRole + 1, HeadingRole, ColorRole
   };
   explicit TxInputsSelectedModel(TxInputsModel* source);

   int rowCount(const QModelIndex & = QModelIndex()) const override;
   int columnCount(const QModelIndex & = QModelIndex()) const override;
   QVariant data(const QModelIndex& index, int role) const override;
   QHash<int, QByteArray> roleNames() const override;
   QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

signals:
   void rowCountChanged();

private slots:
   void onSelectionChanged();

private:
   QVariant getData(int row, int col) const;
   QColor dataColor(int row, int col) const;

private:
   enum Columns { ColumnTxId, ColumnTxOut, ColumnBalance };
   TxInputsModel* source_{ nullptr };
   const QMap<int, QString> header_;
   QUTXOList* selection_{ nullptr };
};



#endif	// TX_INPUTS_SELECTED_MODEL_H
