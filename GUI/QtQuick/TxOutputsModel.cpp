/*

***********************************************************************************
* Copyright (C) 2020 - 2022, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "TxOutputsModel.h"
#include <spdlog/spdlog.h>
#include "Address.h"
#include "BTCNumericTypes.h"
#include "ColorScheme.h"
#include "TxListModel.h"
#include "Utils.h"

namespace {
   static const QHash<int, QByteArray> kRoles{
      {TxOutputsModel::TableDataRole, "tableData"},
      {TxOutputsModel::HeadingRole, "heading"},
      {TxOutputsModel::ColorRole, "dataColor"}
   };
}

TxOutputsModel::TxOutputsModel(const std::shared_ptr<spdlog::logger>& logger
   , QObject* parent, bool readOnly)
   : QAbstractTableModel(parent), logger_(logger), readOnly_(readOnly)
   , header_{ tr("Output Address"), {}, tr("Amount (BTC)"), {} }
{
   connect(this, &TxOutputsModel::modelReset,
           this, &TxOutputsModel::rowCountChanged);
   connect(this, &TxOutputsModel::rowsInserted,
           this, &TxOutputsModel::rowCountChanged);
   connect(this, &TxOutputsModel::rowsRemoved,
           this, &TxOutputsModel::rowCountChanged);
}

int TxOutputsModel::rowCount(const QModelIndex &) const
{
   return data_.size() + 1;
}

int TxOutputsModel::columnCount(const QModelIndex &) const
{
   return readOnly_ ? header_.size() - 1 : header_.size();
}

QVariant TxOutputsModel::data(const QModelIndex& index, int role) const
{
   switch (role) {
   case TableDataRole:
      return getData(index.row(), index.column());
   case HeadingRole:
      return (index.row() == 0);
   case ColorRole:
      return dataColor(index.row());
   default: break;
   }
   return QVariant();
}

QHash<int, QByteArray> TxOutputsModel::roleNames() const
{
   return kRoles;
}

QVariant TxOutputsModel::headerData(int section, Qt::Orientation orientation, int role) const
{
   if (orientation == Qt::Orientation::Horizontal) {
      return header_.at(section);
   }
   return QVariant();
}

double TxOutputsModel::totalAmount() const
{
   double result = 0.0;
   for (const auto& entry : data_) {
      result += entry.amount;
   }
   return result;
}

std::vector<std::shared_ptr<Armory::Signer::ScriptRecipient>> TxOutputsModel::recipients() const
{
   std::vector<std::shared_ptr<Armory::Signer::ScriptRecipient>> result;
   for (const auto& entry : data_) {
      result.emplace_back(entry.address.getRecipient(bs::XBTAmount(entry.amount)));
   }
   return result;
}

void TxOutputsModel::clearOutputs()
{
   beginResetModel();
   data_.clear();
   endResetModel();
}

QStringList TxOutputsModel::getOutputAddresses() const
{
   QStringList res;
   for (int row = 1; row < rowCount(); row++) {
      res.append(getData(row, 0).toString());
   }
   return res;
}

QList<double> TxOutputsModel::getOutputAmounts() const
{
   QList<double> res;
   for (int row = 1; row < rowCount(); row++) {
      res.append(data_.at(row - 1).amount);
   }
   return res;
}

void TxOutputsModel::setOutputsFrom(QTxDetails* tx)
{
   beginResetModel();
   data_.clear();
   for (const auto& out : tx->outputData()) {
      data_.push_back({out.first, out.second});
   }
   endResetModel();
   logger_->debug("[{}] {} entries", __func__, data_.size());
}

void TxOutputsModel::addOutput(const QString& address, double amount, bool isChange)
{
   bs::Address addr;
   try {
      addr = bs::Address::fromAddressString(address.toStdString());
   }
   catch (const std::exception&) {
      return;
   }
   QMetaObject::invokeMethod(this, [this, addr, amount, isChange] {
      Entry entry{ addr, amount, isChange };
      beginInsertRows(QModelIndex(), rowCount(), rowCount());
      data_.emplace_back(std::move(entry));
      endInsertRows();
      });
}

void TxOutputsModel::delOutput(int row)
{
   if (readOnly_) {
      return;
   }
   if (row == 0) {
      beginResetModel();
      data_.clear();
      endResetModel();
   }
   else {
      beginRemoveRows(QModelIndex(), row, row);
      data_.erase(data_.cbegin() + row - 1);
      endRemoveRows();
   }
}

QVariant TxOutputsModel::getData(int row, int col) const
{
   if (row == 0) {
      return header_.at(col);
   }
   const auto& entry = data_.at(row - 1);
   switch (col) {
   case 0:
      return QString::fromStdString(entry.address.display());
   case 1:
      return entry.isChange ? tr("(change)") : QString{};
   case 2:
      return gui_utils::xbtToQString(entry.amount);
   default: break;
   }
   return {};
}

QColor TxOutputsModel::dataColor(int row) const
{
   if (row == 0) {
      return ColorScheme::tableHeaderColor;
   }
   return ColorScheme::tableTextColor;
}
