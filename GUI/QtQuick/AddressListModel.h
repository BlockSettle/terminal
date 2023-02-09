/*

***********************************************************************************
* Copyright (C) 2020 - 2022, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef ADDRESS_LIST_MODEL_H
#define ADDRESS_LIST_MODEL_H

#include <QAbstractTableModel>
#include <QObject>
#include <QVariant>
#include <QColor>
#include "Address.h"
#include "BinaryData.h"

namespace spdlog {
   class logger;
}

class QmlAddressListModel : public QAbstractTableModel
{
   Q_OBJECT
public:
   enum TableRoles { TableDataRole = Qt::UserRole + 1, HeadingRole, FirstColRole, ColorRole, AddressTypeRole };
   QmlAddressListModel(const std::shared_ptr<spdlog::logger>&, QObject* parent = nullptr);

   int rowCount(const QModelIndex & = QModelIndex()) const override;
   int columnCount(const QModelIndex & = QModelIndex()) const override;
   QVariant data(const QModelIndex& index, int role) const override;
   QHash<int, QByteArray> roleNames() const override;

   void addRow(const std::string& walletId, const QVector<QString>&);
   void addRows(const std::string& walletId, const QVector<QVector<QString>>&);
   void updateRow(const BinaryData& addr, uint64_t bal, uint32_t nbTx);
   void reset(const std::string& expectedWalletId);

private:
   std::shared_ptr<spdlog::logger>  logger_;
   const QStringList          header_;
   QVector<QVector<QString>>  table_;
   std::vector<bs::Address>   addresses_;

   struct PendingBalance {
      uint64_t    balance{ 0 };
      uint32_t    nbTx{ 0 };
   };
   std::map<BinaryData, PendingBalance> pendingBalances_;
   std::string expectedWalletId_;
};

#endif	// ADDRESS_LIST_MODEL_H
