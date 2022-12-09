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
#include <QObject>
#include <QVariant>

namespace spdlog {
   class logger;
}

class TxListModel : public QAbstractTableModel
{
   Q_OBJECT
public:
   enum TableRoles { TableDataRole = Qt::UserRole + 1, HeadingRole };
   TxListModel(const std::shared_ptr<spdlog::logger>&, QObject* parent = nullptr);

   int rowCount(const QModelIndex & = QModelIndex()) const override;
   int columnCount(const QModelIndex & = QModelIndex()) const override;
   QVariant data(const QModelIndex& index, int role) const override;
   QHash<int, QByteArray> roleNames() const override;

   void addRow(const QVector<QString>&);
   void clear();

private:
   QVector<QVector<QString>> table;
};

#endif	// TX_LIST_MODEL_H
