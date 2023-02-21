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
#include "Utils.h"

namespace {
   static const QHash<int, QByteArray> kRoles{
      {TxOutputsModel::TableDataRole, "tableData"},
      {TxOutputsModel::HeadingRole, "heading"},
      {TxOutputsModel::ColorRole, "dataColor"}
   };
}

TxOutputsModel::TxOutputsModel(const std::shared_ptr<spdlog::logger>& logger, QObject* parent)
   : QAbstractTableModel(parent), logger_(logger)
   , header_{ tr("Output Address"), tr("Amount (BTC)"), {} }
{ }

int TxOutputsModel::rowCount(const QModelIndex &) const
{
   return data_.size() + 1;
}

int TxOutputsModel::columnCount(const QModelIndex &) const
{
   return header_.size();
}

QVariant TxOutputsModel::data(const QModelIndex& index, int role) const
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
   return QVariant();
}

QHash<int, QByteArray> TxOutputsModel::roleNames() const
{
   return kRoles;
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
   for(int row=1; row<rowCount(); row++)
      res.append(getData(row,0).toString());

   return res;
}

QList<double> TxOutputsModel::getOutputAmounts() const
{
   QList<double> res;
   for(int row=1; row<rowCount(); row++)
       res.append(data_.at(row - 1).amount);

    return res;
}

void TxOutputsModel::addOutput(const QString& address, double amount)
{
   bs::Address addr;
   try {
      addr = bs::Address::fromAddressString(address.toStdString());
   }
   catch (const std::exception&) {
      return;
   }
   Entry entry{ addr, amount };
   beginInsertRows(QModelIndex(), rowCount(), rowCount());
   data_.emplace_back(std::move(entry));
   endInsertRows();
}

void TxOutputsModel::delOutput(int row)
{
   beginRemoveRows(QModelIndex(), row, row);
   data_.erase(data_.cbegin() + row - 1);
   endRemoveRows();
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
      return gui_utils::normalizedSatoshiToQString(entry.amount);
   default: break;
   }
   return {};
}

QColor TxOutputsModel::dataColor(int row, int col) const
{
    if (row == 0) {
       return ColorScheme::tableHeaderColor;
    }
    return ColorScheme::tableTextColor;
}
