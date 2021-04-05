/*

***********************************************************************************
* Copyright (C) 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __UTXO_MODEL_INTERFACE_H__
#define __UTXO_MODEL_INTERFACE_H__

#include <QAbstractTableModel>

class UtxoModelInterface: public QAbstractTableModel
{
Q_OBJECT

public:
   UtxoModelInterface(QObject* parent);
   ~UtxoModelInterface() noexcept override = default;

   void enableRows(bool flag = true);

   Qt::ItemFlags flags(const QModelIndex &index) const override;
   QVariant data(const QModelIndex & index, int role = Qt::DisplayRole) const override;

protected:
   bool rowsEnabled_ = true;
};

#endif // __UTXO_MODEL_INTERFACE_H__
