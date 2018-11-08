#include "AddressDetailsWidget.h"
#include "ui_AddressDetailsWidget.h"
#include "UiUtils.h"

#include <QDateTime>
#include <QDebug>

AddressDetailsWidget::AddressDetailsWidget(QWidget *parent) 
   : QWidget(parent)
   , ui_(new Ui::AddressDetailsWidget)
{
   ui_->setupUi(this);

   // sets column resizing to fixed
   //ui_->treeAddressTransactions->header()->setSectionResizeMode(QHeaderView::Fixed);

   // tell CustomTreeWidget for which column the cursor becomes a hand cursor
   ui_->treeAddressTransactions->handCursorColumns_.append(colTxId);
   // allow TxId column to be copied to clipboard with right click
   ui_->treeAddressTransactions->copyToClipboardColumns_.append(colTxId);

   connect(ui_->treeAddressTransactions, &QTreeWidget::itemClicked,
           this, &AddressDetailsWidget::onTxClicked);

}

AddressDetailsWidget::~AddressDetailsWidget() {
    delete ui_;
}

// Initialize the widget and related widgets (block, address, Tx)
void AddressDetailsWidget::init(const std::shared_ptr<ArmoryConnection> &armory,
                                const std::shared_ptr<spdlog::logger> &inLogger)
{
   armory_ = armory;
   logger_ = inLogger;
   connect(armory_.get(), &ArmoryConnection::refresh, this, &AddressDetailsWidget::OnRefresh, Qt::QueuedConnection);
}

void AddressDetailsWidget::setAddrVal(const bs::Address& inAddrVal) {
   addrVal = inAddrVal;
}

void AddressDetailsWidget::loadWallet() {
   // Armory can't directly take an address and return all the required data.
   // Work around this by creating a dummy wallet, adding the explorer address,
   // registering the wallet, and getting the required data.
   dummyWallet.addAddress(addrVal);
   dummyWalletID = dummyWallet.RegisterWallet(armory_, true);
}

void AddressDetailsWidget::loadTransactions() {
   CustomTreeWidget *tree = ui_->treeAddressTransactions;
   tree->clear();

   uint64_t totSpent_ = 0;
   uint64_t totRcvd_ = 0;
   // here's the code to add data to the address tree, the tree.
   for(const auto& txAdd_ : txEntryHashSet_) {
      QTreeWidgetItem *item = new QTreeWidgetItem(tree);

      // Get fees & fee/byte by looping through the prev Tx set and calculating.
      uint64_t totIn = 0;
      for(size_t r = 0; r < txMap_[txAdd_.first].getNumTxIn(); ++r) {
         TxIn in = txMap_[txAdd_.first].getTxInCopy(r);
         OutPoint op = in.getOutPoint();
         const auto &prevTx = prevTxMap_[op.getTxHash()];
         if (prevTx.isInitialized()) {
            TxOut prevOut = prevTx.getTxOutCopy(op.getTxOutIndex());
            totIn += prevOut.getValue();
         }
      }
      uint64_t fees = totIn - txMap_[txAdd_.first].getSumOfOutputs();
      double feePerByte = (double)fees / (double)txMap_[txAdd_.first].getSize();

      // Populate the transaction entries.
      item->setText(0, UiUtils::displayDateTime(QDateTime::fromTime_t(txAdd_.second.txTime))); // date
      item->setText(1, QString::fromStdString(txAdd_.first.toHexStr())); // tx id
      item->setText(2, QString::number(armory_->getConfirmationsNumber(txAdd_.second.blockNum))); // confirmations
      item->setText(3, QString::number(txMap_[txAdd_.first].getNumTxIn())); // inputs #
      item->setText(4, QString::number(txMap_[txAdd_.first].getNumTxOut())); // outputs #
      item->setText(5, QString::number(txAdd_.second.value / BTCNumericTypes::BalanceDivider,
                                       'f',
                                       BTCNumericTypes::default_precision)); // output
      item->setText(6, QString::number((double)fees / BTCNumericTypes::BalanceDivider,
                                       'f',
                                       BTCNumericTypes::default_precision)); // fees
      item->setText(7, QString::number(nearbyint(feePerByte))); // fee per byte
      item->setText(8, QString::number((double)txMap_[txAdd_.first].getSize() / 1000.0, 'f', 3)); // size

      // Check the total received or sent.
      if(txAdd_.second.value > 0) {
         totRcvd_ += txAdd_.second.value;
      }
      else {
         totSpent_ -= txAdd_.second.value; // Negative, so fake that out.
      }

      // Misc. cleanup.
      setConfirmationColor(item);
      // disabled as per Scott's request
      //setOutputColor(item);
      tree->addTopLevelItem(item);
   }
   // Set up the display for total rcv'd/spent.
   ui_->totalReceived->setText(QString::number((double)totRcvd_ / BTCNumericTypes::BalanceDivider,
                                               'f',
                                               BTCNumericTypes::default_precision));
   ui_->totalSent->setText(QString::number((double)totSpent_ / BTCNumericTypes::BalanceDivider,
                                           'f',
                                           BTCNumericTypes::default_precision));

   // Misc. cleanup
   tree->resizeColumns();
}

// This function sets the confirmation column to the correct color based
// on the number of confirmations that are set inside the column.
void AddressDetailsWidget::setConfirmationColor(QTreeWidgetItem *item) {
   auto conf = item->text(colConfs).toInt();
   QBrush brush;
   if (conf == 0) {
      brush.setColor(Qt::red);
   }
   else if (conf >= 1 && conf <= 5) {
      brush.setColor(Qt::darkYellow);
   }
   else {
      brush.setColor(Qt::darkGreen);
   }
   item->setForeground(colConfs, brush);
}

// This functions sets the output column color based on positive or negative value.
void AddressDetailsWidget::setOutputColor(QTreeWidgetItem *item) {
   auto output = item->text(colOutput).toDouble();
   QBrush brush;
   if (output > 0) {
      brush.setColor(Qt::darkGreen);
   }
   else if (output < 0) {
      brush.setColor(Qt::red);
   }
   else {
      brush.setColor(Qt::white);
   }
   item->setForeground(colOutput, brush);
}

void AddressDetailsWidget::onTxClicked(QTreeWidgetItem *item, int column) {
   // user has clicked the transaction column of the item so
   // send a signal to ExplorerWidget to open TransactionDetailsWidget
   if (column == colTxId) {
      emit(transactionClicked(item->text(colTxId)));
   }
}

// Used in callback - FIX DESCRIPTION
void AddressDetailsWidget::getTxData(AsyncClient::LedgerDelegate delegate) {
   // The callback that gets the transaction objects for an address. This is
   // where we get the data we need. (FIX)
   const auto &cbCollectPrevTXs = [this](std::vector<Tx> prevTxs) {
      for (const auto &prevTx : prevTxs) {
         const auto &txHash = prevTx.getThisHash();
         prevTxHashSet.erase(txHash); // If empty, use this to make a relevant call.
         prevTxMap_[txHash] = prevTx;
      }

      // We're finally ready to display all the transactions.
      loadTransactions();
   };

   const auto &cbCollectTXs = [this, cbCollectPrevTXs](std::vector<Tx> txs) {
      for (const auto &tx : txs) {
         const auto &prevTxHash = tx.getThisHash();
         txHashSet.erase(prevTxHash); // If empty, use this to make a relevant call.
         txMap_[prevTxHash] = tx;

         // While here, we need to get the prev Tx with the UTXO being spent.
         // This is done so that we can calculate fees later.
         for (size_t i = 0; i < tx.getNumTxIn(); i++) {
            TxIn in = tx.getTxInCopy(i);
            OutPoint op = in.getOutPoint();
            const auto &itTX = prevTxMap_.find(op.getTxHash());
            if(itTX == prevTxMap_.end()) {
               prevTxHashSet.insert(op.getTxHash());
//               prevTxEntryHashSet_[searchHash_] = bs::convertTXEntry(entry);
            }
         }
      }
      if(!prevTxHashSet.empty()) {
         armory_->getTXsByHash(prevTxHashSet, cbCollectPrevTXs);
      }
   };

   // Callback to process ledger entries (pages) from the ledger delegate.
   const auto &cbLedger = [this, cbCollectTXs]
                          (std::vector<ClientClasses::LedgerEntry> entries) {
      // Get the hash and TXEntry object for each relevant Tx hash.
      for(const auto &entry : entries) {
         BinaryData searchHash_(entry.getTxHash());
         const auto &itTX = txMap_.find(searchHash_);
         if(itTX == txMap_.end()) {
            txHashSet.insert(searchHash_);
            txEntryHashSet_[searchHash_] = bs::convertTXEntry(entry);
         }
      }
      if(!txHashSet.empty()) {
         armory_->getTXsByHash(txHashSet, cbCollectTXs);
      }
   };
   delegate.getHistoryPage(0, cbLedger);  // ? should we use more than 0 pageId?
}

// Called when Armory has finished registering a wallet.
void AddressDetailsWidget::OnRefresh(std::vector<BinaryData> ids)
{
   // Make sure Armory is telling us about our wallet. Refreshes occur for
   // multiple item types.
   const auto &it = std::find(ids.begin(), ids.end(), dummyWalletID);
   if (it == ids.end()) {
      return;
   }

   logger_->debug("[AddressDetailsWidget::OnRefresh] get refresh command");

   ui_->addressId->setText(addrVal.display());
   // Once this callback runs, the data is safe to grab.
   const auto &cbBalance = [this](std::vector<uint64_t> balances) {
      double curBalance = dummyWallet.GetTxBalance(balances[0]);
      ui_->balance->setText(QString::number(curBalance, 'f', 8));

      const auto &cbLedgerDelegate = [this](AsyncClient::LedgerDelegate delegate) {
         // UI changes in a non-main thread will trigger a crash. Invoke a new
         // thread to handle the received data. (UI changes happen eventually.)
         QMetaObject::invokeMethod(this, "getTxData", Qt::QueuedConnection
            , Q_ARG(AsyncClient::LedgerDelegate, delegate));
      };
      if(!armory_->getLedgerDelegateForAddress(dummyWallet.GetWalletId(),
                                               addrVal,
                                               cbLedgerDelegate)) {
//         ui_->labelError->setText(tr("Error loading address info"));
      }
   };
   dummyWallet.UpdateBalanceFromDB(cbBalance);
}
