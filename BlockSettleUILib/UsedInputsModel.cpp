/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "UsedInputsModel.h"

#include "BtcUtils.h"
#include "TxClasses.h"
#include "UiUtils.h"

#include <QSize>
#include <unordered_map>

UsedInputsModel::UsedInputsModel(QObject* parent)
   : QAbstractTableModel{parent}
{}

int UsedInputsModel::rowCount(const QModelIndex & parent) const
{
   if (parent.isValid()) {
      return 0;
   }

   return (int)inputs_.size();
}

int UsedInputsModel::columnCount(const QModelIndex & parent) const
{
   return ColumnCount;
}

void UsedInputsModel::clear()
{
   beginResetModel();
   inputs_.clear();
   endResetModel();
}

QVariant UsedInputsModel::data(const QModelIndex & index, int role) const
{
   if (role == Qt::SizeHintRole && index.column() == 1) {
      return QSize(50, 14); // workaround used here
                            // TODO: move "Delete output button"
                            // from CreateTransactionDialogAdvanced::onOutputsInserted to model delegate
   }

   switch (role) {
   case Qt::TextAlignmentRole:
      return int(Qt::AlignLeft | Qt::AlignVCenter);
   case Qt::DisplayRole:
      return getRowData(index.column(), inputs_[index.row()]);
   }
   return QVariant{};
}

QVariant UsedInputsModel::headerData(int section, Qt::Orientation orientation, int role) const
{
   if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
   {
      switch (section)
      {
         case ColumnAddress:
            return tr("Address");

         case ColumnTxCount:
            return tr("#Tx");

         case ColumnBalance:
            return tr("Balance");
      }
   }

   return QVariant{};
}

void UsedInputsModel::updateInputs(const std::vector<UTXO>& usedInputs)
{
   std::unordered_map<std::string, InputData> loadedInputs;

   for (const auto& utx : usedInputs) {
      const auto address = bs::Address::fromUTXO(utx);
      const auto addrStr = address.display();

      auto it = loadedInputs.find(addrStr);
      if (it == loadedInputs.end()) {
         loadedInputs.emplace(addrStr
            , InputData{QString::fromStdString(addrStr), 1, UiUtils::amountToBtc(utx.getValue())} );
      } else {
         it->second.txCount++;
         it->second.balance += UiUtils::amountToBtc(utx.getValue());
      }
   }

   beginResetModel();

   inputs_.clear();
   inputs_.reserve(loadedInputs.size());
   for (const auto& i : loadedInputs) {
      inputs_.emplace_back( i.second );
   }

   endResetModel();
}

QVariant UsedInputsModel::getRowData(const int column, const InputData& data) const
{
   switch(column) {
   case ColumnAddress:
      return data.address;
   case ColumnTxCount:
      return data.txCount;
   case ColumnBalance:
      return UiUtils::displayAmount(data.balance);
   }

   return QVariant{};
}
