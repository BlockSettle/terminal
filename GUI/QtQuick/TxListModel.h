/*

***********************************************************************************
* Copyright (C) 2020 - 2022, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef TX_LIST_MODEL_H
#define TX_LIST_MODEL_H

#include <memory>
#include <QAbstractTableModel>
#include <QColor>
#include <QObject>
#include <QVariant>
#include "ArmoryConnection.h"
#include "Wallets/SignerDefs.h"

namespace spdlog {
   class logger;
}

class TxListModel : public QAbstractTableModel
{
   Q_OBJECT
public:
   enum TableRoles { TableDataRole = Qt::UserRole + 1, HeadingRole, ColorRole, WidthRole };
   TxListModel(const std::shared_ptr<spdlog::logger>&, QObject* parent = nullptr);

   int rowCount(const QModelIndex & = QModelIndex()) const override;
   int columnCount(const QModelIndex & = QModelIndex()) const override;
   QVariant data(const QModelIndex& index, int role) const override;
   QHash<int, QByteArray> roleNames() const override;

   void prependRow(const bs::TXEntry&);
   void addRow(const bs::TXEntry&);
   void addRows(const std::vector<bs::TXEntry>&);
   void clear();
   void setTxComment(const std::string& txHash, const std::string& comment);
   void setWalletName(const std::string& walletId, const std::string& walletName);
   void setDetails(const bs::sync::TXWalletDetails&);
   void setCurrentBlock(uint32_t);

private:
   QString getData(int row, int col) const;
   QColor dataColor(int row, int col) const;
   float colWidth(int col) const;
   QString walletNameById(const std::string&) const;
   QString txType(int row) const;
   QString txFlag(int row) const;

private:
   std::shared_ptr<spdlog::logger>  logger_;
   const QStringList header_;
   std::vector<bs::TXEntry> data_;
   std::unordered_map<std::string, std::string> txComments_;
   std::unordered_map<std::string, std::string> walletNames_;
   std::map<int, bs::sync::TXWalletDetails>  txDetails_;
   uint32_t curBlock_;
};

#endif	// TX_LIST_MODEL_H
