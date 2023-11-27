#include "TxInputsSelectedModel.h"
#include "ColorScheme.h"
#include "Utils.h"

namespace {
   static const QHash<int, QByteArray> kRoles{
      { TxInputsSelectedModel::TableDataRole, "tableData"},
      { TxInputsSelectedModel::HeadingRole, "heading" },
      { TxInputsSelectedModel::ColorRole, "dataColor" }
   };
}

TxInputsSelectedModel::TxInputsSelectedModel(TxInputsModel* source)
   : QAbstractTableModel(source), source_(source)
   , header_{ {ColumnTxId, tr("TX Hash")}, {ColumnTxOut, tr("OutNdx")},
         {ColumnBalance, tr("Balance (BTC)")} }
{
   selection_ = source_->getSelection();
   connect(source_, &TxInputsModel::selectionChanged, this, &TxInputsSelectedModel::onSelectionChanged);
}

void TxInputsSelectedModel::onSelectionChanged()
{
   beginResetModel();
   selection_ = source_->getSelection();
   endResetModel();
   emit rowCountChanged();
}

int TxInputsSelectedModel::rowCount(const QModelIndex&) const
{
   return (selection_ == nullptr) ? 1 : selection_->rowCount() + 1;
}

int TxInputsSelectedModel::columnCount(const QModelIndex&) const
{
   return header_.size();
}

QVariant TxInputsSelectedModel::data(const QModelIndex& index, int role) const
{
   switch (role) {
   case TableDataRole:
      return getData(index.row(), index.column());
   case HeadingRole:
      return (index.row() == 0);
   case ColorRole:
      return dataColor(index.row(), index.column());
   default: break;
   }
   return {};
}

QHash<int, QByteArray> TxInputsSelectedModel::roleNames() const
{
   return kRoles;
}

QVariant TxInputsSelectedModel::headerData(int section, Qt::Orientation orientation, int role) const
{
   return header_[section];
}

QVariant TxInputsSelectedModel::getData(int row, int col) const
{
   if (row == 0) {
      return header_[col];
   }
   if (!selection_) {
      return {};
   }
   if (selection_->rowCount() < row) {
      return {};
   }
   const auto& entry = selection_->data().at(row - 1);
   switch (col) {
   case ColumnTxId: {
      const auto& txId = entry->utxo().getTxHash().toHexStr(true);
      std::string str;
      if (txId.size() > 40) {
         str = txId.substr(0, 20) + "..." + txId.substr(txId.size() - 21, 20);
      }
      else {
         str = txId;
      }
      return QString::fromStdString(str);
   }
   case ColumnTxOut:
      return QString::number(entry->utxo().getTxOutIndex());
   case ColumnBalance:
      return gui_utils::satoshiToQString(entry->utxo().getValue());
   default: break;
   }
   return {};
}

QColor TxInputsSelectedModel::dataColor(int row, int col) const
{
   if (row == 0) {
      return ColorScheme::tableHeaderColor;
   }
   return ColorScheme::tableTextColor;
}
