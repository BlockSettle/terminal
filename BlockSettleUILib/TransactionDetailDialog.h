/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __TRANSACTION_DETAIL_DIALOG_H__
#define __TRANSACTION_DETAIL_DIALOG_H__

#include "BinaryData.h"

#include <QDialog>
#include <memory>

#include "TransactionsViewModel.h"
#include "ValidityFlag.h"

namespace Ui {
    class TransactionDetailDialog;
}
namespace bs {
   namespace sync {
      namespace hd {
         class CCLeaf;
      }
      class Wallet;
      class WalletsManager;
   }
}
class ArmoryConnection;
class QTreeWidgetItem;
class TxOut;

//sublcassing this Dialog is not a good idea because of how it handles minimumSize

class TransactionDetailDialog : public QDialog
{
Q_OBJECT

public:
   TransactionDetailDialog(const TransactionPtr &tvi, const std::shared_ptr<bs::sync::WalletsManager> &
      , const std::shared_ptr<ArmoryConnection> &, QWidget* parent = nullptr);
   ~TransactionDetailDialog() override;
   virtual QSize minimumSizeHint() const override;
   QSize minimumSize() const;

   static const int extraTreeWidgetColumnMargin = 10;
   static const int minHeightAtRendering = 500;

private:
   using WalletsSet = std::set<std::shared_ptr<bs::sync::Wallet>>;
   void addAddress(TxOut out, bool isOutput, bool isInternalTx
      , const BinaryData& txHash, const WalletsSet &inputWallets);
   QString getScriptType(const TxOut &);

private:
   std::unique_ptr<Ui::TransactionDetailDialog> ui_;
   std::shared_ptr<bs::sync::WalletsManager>    walletsManager_;
   QTreeWidgetItem   *itemSender_ = nullptr;
   QTreeWidgetItem   *itemReceiver_ = nullptr;
   ValidityFlag validityFlag_;
   std::shared_ptr<bs::sync::hd::CCLeaf>  ccLeaf_;
};

#endif // __TRANSACTION_DETAIL_DIALOG_H__
