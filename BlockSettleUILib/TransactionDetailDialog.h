#ifndef __TRANSACTION_DETAIL_DIALOG_H__
#define __TRANSACTION_DETAIL_DIALOG_H__

#include "BinaryData.h"

#include <QDialog>
#include <memory>

#include "TransactionsViewModel.h"

namespace Ui {
    class TransactionDetailDialog;
}
namespace bs {
   namespace sync {
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
   void addAddress(const std::shared_ptr<bs::sync::Wallet> &, const TxOut& out,
      bool isOutput, bool isInternalTx, const BinaryData& txHash, const WalletsSet *inputWallets);
   QString getScriptType(const TxOut &);

private:
   std::unique_ptr<Ui::TransactionDetailDialog> ui_;
   std::shared_ptr<bs::sync::WalletsManager>    walletsManager_;
   QTreeWidgetItem   *itemSender_ = nullptr;
   QTreeWidgetItem   *itemReceiver_ = nullptr;
};

#endif // __TRANSACTION_DETAIL_DIALOG_H__
