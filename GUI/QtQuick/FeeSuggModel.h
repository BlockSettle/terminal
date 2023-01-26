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

#include <map>
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

   Q_PROPERTY(int rowCount READ rowCount NOTIFY rowCountChanged)
public:
   enum TableRoles { TextRole = Qt::DisplayRole, BlocksRole = Qt::UserRole, TimeRole, ValueRole };
   FeeSuggestionModel(const std::shared_ptr<spdlog::logger>&, QObject* parent = nullptr);

   int rowCount(const QModelIndex & = QModelIndex()) const override;
   int columnCount(const QModelIndex & = QModelIndex()) const override;
   QVariant data(const QModelIndex& index, int role) const override;
   QHash<int, QByteArray> roleNames() const override;

   static std::map<uint32_t, QString> feeLevels();
   struct FeeSuggestion {
      uint32_t nbBlocks;
      QString  estTime;
      float    satoshis;
   };
   void addRows(const std::map<uint32_t, float>&);
   void clear();

private:
   std::shared_ptr<spdlog::logger>  logger_;
   std::vector<FeeSuggestion> data_;

signals:
   void rowCountChanged();
};

#endif	// FEE_SUGG_MODEL_H
