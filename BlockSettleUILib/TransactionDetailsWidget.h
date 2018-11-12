#ifndef __TRANSACTIONDETAILSWIDGET_H__
#define __TRANSACTIONDETAILSWIDGET_H__

#include "ArmoryConnection.h"
#include "BinaryData.h"
#include "TxClasses.h"
#include "spdlog/logger.h"

#include <QWidget>

namespace spdlog {
   class logger;
}

namespace Ui {
   class TransactionDetailsWidget;
}
class CustomTreeWidget;
class QTreeWidgetItem;
class QTreeWidget;

class TransactionDetailsWidget : public QWidget
{
   Q_OBJECT

public:
   explicit TransactionDetailsWidget(QWidget *parent = nullptr);
   ~TransactionDetailsWidget();

   void init(const std::shared_ptr<ArmoryConnection> &armory,
             const std::shared_ptr<spdlog::logger> &inLogger);
   void setTxGUIValues();
   void loadInputs();
   void populateTransactionWidget(BinaryData inHex,
                                  const bool& firstPass = true);

    enum TxTreeColumns {
       colType = 0,
       colAddressId = 1,
       colAmount = 2,
       colWallet
    };

signals:
   void addressClicked(QString addressId);

protected slots:
   void onAddressClicked(QTreeWidgetItem *item, int column);

protected:
   void loadTreeIn(CustomTreeWidget *tree);
   void loadTreeOut(CustomTreeWidget *tree);

private:
   Ui::TransactionDetailsWidget *ui_;
   std::shared_ptr<ArmoryConnection>   armory_;
   std::shared_ptr<spdlog::logger> logger_;

   // The Tx being analyzed.
   Tx curTx_;

   // Data captured from callback to get a Tx's inputs.
   std::map<BinaryData, Tx> prevTxMap_; // A Tx's previous Tx hash / Tx map (fee stuff).
   std::set<BinaryData> prevTxHashSet_; // Prev Tx hashes for a Tx (fee calc).

   void processTxData(Tx tx);

   QTreeWidgetItem * createItem(QTreeWidget *tree, QString type,
                                QString address, QString amount,
                                QString wallet);
   QTreeWidgetItem * createItem(QTreeWidgetItem *parentItem, QString type,
                                QString address, QString amount,
                                QString wallet);

   // Functions used by callbacks to copy relevant callback data.
   void setTxs(const std::vector<Tx> inTxs,
               const std::map<BinaryData, std::set<uint32_t>> inIndices);
   void getHeaderData(const BinaryData& inHeader);
};

#endif // TRANSACTIONDETAILSWIDGET_H
