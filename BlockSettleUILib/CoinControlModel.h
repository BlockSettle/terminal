/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __COIN_CONTROL_MODEL_H__
#define __COIN_CONTROL_MODEL_H__

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <QAbstractItemModel>

namespace bs {
   namespace sync {
      class Wallet;
   }
}
class CoinControlNode;
class SelectedTransactionInputs;

struct UTXO;

class CoinControlModel : public QAbstractItemModel
{
Q_OBJECT
public:
   enum Column
   {
      ColumnName,
      ColumnUTXOCount,
      ColumnComment,
      ColumnBalance,
      ColumnsCount
   };

public:
   CoinControlModel(const std::shared_ptr<SelectedTransactionInputs>& selectedInputs, QObject* parent = nullptr);
   ~CoinControlModel() override = default;

   int columnCount(const QModelIndex & parent = QModelIndex()) const override;
   int rowCount(const QModelIndex & parent = QModelIndex()) const override;

   Qt::ItemFlags flags(const QModelIndex & index) const override;
   QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

   QVariant data(const QModelIndex& index, int role) const override;
   bool setData(const QModelIndex & index, const QVariant & value, int role) override;

   QModelIndex index(int row, int column, const QModelIndex & parent = QModelIndex()) const override;

   QModelIndex parent(const QModelIndex& child) const override;
   bool hasChildren(const QModelIndex& parent = QModelIndex()) const override;

   size_t GetSelectedTransactionsCount() const;
   QString GetSelectedBalance() const;
   QString GetTotalBalance() const;

   void ApplySelection(const std::shared_ptr<SelectedTransactionInputs>& selectedInputs);

   void clearSelection();
   void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override;
signals:
   void selectionChanged();

public slots:
   void selectAll(int sel);

private:
   CoinControlNode* getNodeByIndex(const QModelIndex& index) const;

   void loadInputs(const std::shared_ptr<SelectedTransactionInputs> &selectedInputs);

private:
   std::shared_ptr<CoinControlNode>    root_, cpfp_;
   std::shared_ptr<bs::sync::Wallet>   wallet_;
   std::unordered_map<std::string, CoinControlNode*> addressNodes_, cpfpNodes_;
};

#endif // __COIN_CONTROL_MODEL_H__
