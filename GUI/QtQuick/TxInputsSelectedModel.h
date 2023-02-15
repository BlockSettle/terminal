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

#include <QSortFilterProxyModel>

#include "TxInputsModel.h"

class TxInputsSelectedModel : public QSortFilterProxyModel
{
   Q_OBJECT
   Q_PROPERTY(int rowCount READ rowCount NOTIFY rowCountChanged)

public:
   explicit TxInputsSelectedModel (QObject *parent = nullptr);

   bool filterAcceptsRow (int sourceRow, const QModelIndex &sourceParent) const;

signals:
   void rowCountChanged ();
};



#endif	// TX_INPUTS_SELECTED_MODEL_H
