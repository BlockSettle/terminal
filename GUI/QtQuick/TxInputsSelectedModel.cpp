#include "TxInputsSelectedModel.h"

TxInputsSelectedModel::TxInputsSelectedModel (QObject *parent)
    : QSortFilterProxyModel (parent)
{
   connect (this, &TxInputsSelectedModel::rowsInserted, this, &TxInputsSelectedModel::rowCountChanged);
   connect (this, &TxInputsSelectedModel::rowsRemoved, this, &TxInputsSelectedModel::rowCountChanged);
}

bool TxInputsSelectedModel::filterAcceptsRow (int sourceRow, const QModelIndex &sourceParent) const
{
   Q_UNUSED(sourceParent)
   const auto inputsModel = qobject_cast<TxInputsModel *> (sourceModel ());
   if (!inputsModel) {
      return false;
   }

   const auto isHeader = (sourceRow == 0);

   const auto selected = inputsModel->data (inputsModel->index (sourceRow, 0),
                                             static_cast<int> (TxInputsModel::SelectedRole)).toBool ();

   return isHeader || selected;
}
