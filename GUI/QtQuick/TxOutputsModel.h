/*

***********************************************************************************
* Copyright (C) 2020 - 2022, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef TX_OUTPUTS_MODEL_H
#define TX_OUTPUTS_MODEL_H

#include <QAbstractTableModel>
#include <QColor>
#include <QList>
#include <QObject>
#include <QVariant>
#include "Address.h"
#include "BinaryData.h"
#include "ScriptRecipient.h"
#include "TxClasses.h"

namespace spdlog {
   class logger;
}

class TxOutputsModel : public QAbstractTableModel
{
   Q_OBJECT
public:
   enum TableRoles { TableDataRole = Qt::UserRole + 1, HeadingRole, WidthRole
      , ColorRole };
   TxOutputsModel(const std::shared_ptr<spdlog::logger>&, QObject* parent = nullptr);

   int rowCount(const QModelIndex & = QModelIndex()) const override;
   int columnCount(const QModelIndex & = QModelIndex()) const override;
   QVariant data(const QModelIndex& index, int role) const override;
   QHash<int, QByteArray> roleNames() const override;

   double totalAmount() const;
   std::vector<std::shared_ptr<Armory::Signer::ScriptRecipient>> recipients() const;

   Q_INVOKABLE void addOutput(const QString& address, double amount);
   Q_INVOKABLE void delOutput(int row);
   Q_INVOKABLE void clearOutputs();
   Q_INVOKABLE QStringList getOutputAddresses() const;
   Q_INVOKABLE QList<double> getOutputAmounts() const;

signals:
   void selectionChanged() const;

private:
   QVariant getData(int row, int col) const;
   QColor dataColor(int row, int col) const;

private:
   std::shared_ptr<spdlog::logger>  logger_;
   const QStringList header_;

   struct Entry {
      bs::Address address;
      double      amount;
   };
   std::vector<Entry>   data_;
};

#endif	// TX_OUTPUTS_MODEL_H
