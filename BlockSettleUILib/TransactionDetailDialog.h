/*

***********************************************************************************
* Copyright (C) 2018 - 2021, BlockSettle AB
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
   TransactionDetailDialog(const TransactionPtr &, QWidget* parent = nullptr);
   ~TransactionDetailDialog() override;
   virtual QSize minimumSizeHint() const override;
   QSize minimumSize() const;

   static const int extraTreeWidgetColumnMargin = 10;
   static const int minHeightAtRendering = 500;

private:
   using WalletsSet = std::set<std::shared_ptr<bs::sync::Wallet>>;
   [[deprecated]] void addAddress(TxOut out, bool isOutput
      , const BinaryData& txHash, const WalletsSet &inputWallets
      , const std::vector<TxOut> &allOutputs = {});
   [[deprecated]] QString getScriptType(const TxOut &);
   void addInputAddress(const bs::sync::AddressDetails &);
   void addOutputAddress(const bs::sync::AddressDetails &);
   void addChangeAddress(const bs::sync::AddressDetails &);
   QString getScriptType(TXOUT_SCRIPT_TYPE);

private:
   std::unique_ptr<Ui::TransactionDetailDialog> ui_;
   std::shared_ptr<bs::sync::WalletsManager>    walletsManager_;
   QTreeWidgetItem   *itemSender_ = nullptr;
   QTreeWidgetItem   *itemReceiver_ = nullptr;
   ValidityFlag validityFlag_;
};

#endif // __TRANSACTION_DETAIL_DIALOG_H__
