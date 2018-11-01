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
   void setTxVal(const QString inTx); // possibly a temporary function to show workflow
   void setTx(const Tx& inTx);
   void setTxGUIValues();
   void getTxsForTxIns();
   void loadInputs();

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
   void loadTree(CustomTreeWidget *tree);

private:
   Ui::TransactionDetailsWidget *ui_;
   std::shared_ptr<ArmoryConnection>   armory_;
   std::shared_ptr<spdlog::logger> logger_;

   // The Tx being analyzed, along with associated block header data.
   Tx curTx;
   uint32_t curTxVersion;
   BinaryData curTxPrevHash;
   BinaryData curTxMerkleRoot;
   uint32_t curTxTimestamp;
   BinaryData curTxDifficulty;
   uint32_t curTxNonce;

   // Data captured from callback to get a TxIn's associated Tx & TxOut indices.
   std::vector<Tx> curTxs;
   std::map<BinaryData, std::set<uint32_t>> curIndices;

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
