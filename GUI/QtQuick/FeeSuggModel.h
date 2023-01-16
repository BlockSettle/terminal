/*

***********************************************************************************
* Copyright (C) 2023, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef FEE_SUGG_MODEL_H
#define FEE_SUGG_MODEL_H

#include <QAbstractTableModel>
#include <QObject>
#include <QVariant>
#include "BinaryData.h"

namespace spdlog {
   class logger;
}

class FeeSuggestionModel : public QAbstractTableModel
{
   Q_OBJECT
public:
   enum TableRoles { TextRole = Qt::DisplayRole, BlocksRole = Qt::UserRole, TimeRole, ValueRole };
   FeeSuggestionModel(const std::shared_ptr<spdlog::logger>&, QObject* parent = nullptr);

   int rowCount(const QModelIndex & = QModelIndex()) const override;
   int columnCount(const QModelIndex & = QModelIndex()) const override;
   QVariant data(const QModelIndex& index, int role) const override;
   QHash<int, QByteArray> roleNames() const override;

   struct FeeSuggestion {
      uint32_t nbBlocks;
      unsigned minutes;
      uint64_t satoshis;
   };
   void addRow(const FeeSuggestion&);
   void clear();

private:
   std::shared_ptr<spdlog::logger>  logger_;
   std::vector<FeeSuggestion> data_;
};

#endif	// FEE_SUGG_MODEL_H
