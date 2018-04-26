#ifndef __TRANSACTION_DETAIL_DIALOG_H__
#define __TRANSACTION_DETAIL_DIALOG_H__

#include <QDialog>

#include <memory>

namespace Ui {
    class TransactionDetailDialog;
}
namespace bs {
   class Wallet;
}
class QTreeWidgetItem;
class WalletsManager;
class PyBlockDataManager;
class TransactionsViewItem;
class TxOut;

class TransactionDetailDialog : public QDialog
{
Q_OBJECT

public:
   TransactionDetailDialog(TransactionsViewItem, const std::shared_ptr<WalletsManager> &
      , const std::shared_ptr<PyBlockDataManager> &, QWidget* parent = nullptr);
   ~TransactionDetailDialog() override = default;

private:
   Ui::TransactionDetailDialog*  ui_;
   std::shared_ptr<WalletsManager> walletsManager_;
   QTreeWidgetItem   *  itemSender = nullptr;
   QTreeWidgetItem   *  itemReceiver = nullptr;

private:
   void addAddress(const std::shared_ptr<bs::Wallet> &, const TxOut& out, bool isOutput, bool isTxOutgoing);
   QString getScriptType(const TxOut &);
};

#endif // __TRANSACTION_DETAIL_DIALOG_H__
