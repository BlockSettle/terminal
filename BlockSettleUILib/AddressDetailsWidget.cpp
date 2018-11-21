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
   dummyWallet_.setLogger(inLogger);
   connect(armory_.get(), &ArmoryConnection::refresh, this, &AddressDetailsWidget::OnRefresh, Qt::QueuedConnection);
}

void AddressDetailsWidget::setAddrVal(const bs::Address& inAddrVal) {
   addrVal_ = inAddrVal;
}

void AddressDetailsWidget::loadWallet() {
   // In case we've been here earlier, clear all the text.
   clearFields();

   // Armory can't directly take an address and return all the required data.
   // Work around this by creating a dummy wallet, adding the explorer address,
   // registering the wallet, and getting the required data.
   dummyWallet_.addAddress(addrVal_);
   dummyWalletID_ = dummyWallet_.RegisterWallet(armory_, true);
}

// The function that gathers all the data to place in the UI.
void AddressDetailsWidget::loadTransactions() {
   CustomTreeWidget *tree = ui_->treeAddressTransactions;
   tree->clear();

   uint64_t totSpent = 0;
   uint64_t totRcvd = 0;

   // Go through each TXEntry object and calculate all required UI data.
   for(const auto& curTXEntry : txEntryHashSet_) {
      QTreeWidgetItem *item = new QTreeWidgetItem(tree);

      // Get fees & fee/byte by looping through the prev Tx set and calculating.
      uint64_t totIn = 0;
      for(size_t r = 0; r < txMap_[curTXEntry.first].getNumTxIn(); ++r) {
         TxIn in = txMap_[curTXEntry.first].getTxInCopy(r);
         OutPoint op = in.getOutPoint();
         const auto &prevTx = prevTxMap_[op.getTxHash()];
         if (prevTx.isInitialized()) {
            TxOut prevOut = prevTx.getTxOutCopy(op.getTxOutIndex());
            totIn += prevOut.getValue();
         }
      }
      uint64_t fees = totIn - txMap_[curTXEntry.first].getSumOfOutputs();
      double feePerByte = (double)fees / (double)txMap_[curTXEntry.first].getSize();

      // Populate the transaction entries.
      item->setText(colDate,
                    UiUtils::displayDateTime(QDateTime::fromTime_t(curTXEntry.second.txTime)));
      item->setText(colTxId, // Flip Armory's TXID byte order: internal -> RPC
                    QString::fromStdString(curTXEntry.first.toHexStr(true)));
      item->setText(colConfs,
                    QString::number(armory_->getConfirmationsNumber(curTXEntry.second.blockNum)));
      item->setText(colInputsNum,
                    QString::number(txMap_[curTXEntry.first].getNumTxIn()));
      item->setText(colOutputsNum,
                    QString::number(txMap_[curTXEntry.first].getNumTxOut()));
      item->setText(colOutputAmt,
                    QString::number(curTXEntry.second.value / BTCNumericTypes::BalanceDivider,
                                    'f',
                                    BTCNumericTypes::default_precision));
      item->setText(colFees,
                    QString::number((double)fees / BTCNumericTypes::BalanceDivider,
                                    'f',
                                    BTCNumericTypes::default_precision));
      item->setText(colFeePerByte,
                    QString::number(nearbyint(feePerByte)));
      item->setText(colTxSize,
                    QString::number((double)txMap_[curTXEntry.first].getSize() / 1000.0, 'f', 3));

      // Check the total received or sent.
      if(curTXEntry.second.value > 0) {
         totRcvd += curTXEntry.second.value;
      }
      else {
         totSpent -= curTXEntry.second.value; // Negative, so fake that out.
      }

      setConfirmationColor(item);
      // disabled as per Scott's request
      //setOutputColor(item);
      tree->addTopLevelItem(item);
   }

   // Set up the display for total rcv'd/spent.
   ui_->totalReceived->setText(QString::number((double)totRcvd / BTCNumericTypes::BalanceDivider,
                                               'f',
                                               BTCNumericTypes::default_precision));
   ui_->totalSent->setText(QString::number((double)totSpent / BTCNumericTypes::BalanceDivider,
                                           'f',
                                           BTCNumericTypes::default_precision));

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

// This functions sets the output column color based on pos or neg value.
void AddressDetailsWidget::setOutputColor(QTreeWidgetItem *item) {
   auto output = item->text(colOutputAmt).toDouble();
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
   item->setForeground(colOutputAmt, brush);
}

void AddressDetailsWidget::onTxClicked(QTreeWidgetItem *item, int column) {
   // user has clicked the transaction column of the item so
   // send a signal to ExplorerWidget to open TransactionDetailsWidget
   if (column == colTxId) {
      emit(transactionClicked(item->text(colTxId)));
   }
}

// Used in refresh. The callback used when getting a ledger delegate (pages)
// from Armory. Processes the data as needed.
void AddressDetailsWidget::getTxData(AsyncClient::LedgerDelegate delegate) {
   // The callback that handles previous Tx objects attached to the TxIn objects
   // and processes them. Once done, the UI can be changed.
   const auto &cbCollectPrevTXs = [this](std::vector<Tx> prevTxs) {
      for (const auto &prevTx : prevTxs) {
         const auto &txHash = prevTx.getThisHash();
         prevTxHashSet_.erase(txHash); // If empty, use this to make a relevant call.
         prevTxMap_[txHash] = prevTx;
      }

      // We're finally ready to display all the transactions.
      loadTransactions();
   };

   // Callback used to process Tx objects obtained from Armory. Used primarily
   // to obtain Tx entries for the TxIn objects we're checking.
   const auto &cbCollectTXs = [this, cbCollectPrevTXs](std::vector<Tx> txs) {
      for (const auto &tx : txs) {
         const auto &prevTxHash = tx.getThisHash();
         txHashSet_.erase(prevTxHash); // If empty, use this to make a relevant call.
         txMap_[prevTxHash] = tx;

         // While here, we need to get the prev Tx with the UTXO being spent.
         // This is done so that we can calculate fees later.
         for (size_t i = 0; i < tx.getNumTxIn(); i++) {
            TxIn in = tx.getTxInCopy(i);
            OutPoint op = in.getOutPoint();
            const auto &itTX = prevTxMap_.find(op.getTxHash());
            if(itTX == prevTxMap_.end()) {
               prevTxHashSet_.insert(op.getTxHash());
            }
         }
      }
      if(!prevTxHashSet_.empty()) {
         armory_->getTXsByHash(prevTxHashSet_, cbCollectPrevTXs);
      }
   };

   // Callback to process ledger entries (pages) from the ledger delegate. Gets
   // Tx entries from Armory.
   const auto &cbLedger = [this, cbCollectTXs]
                          (ReturnMessage<std::vector<ClientClasses::LedgerEntry>> entries) {
      try {
         auto le = entries.get();
         // Get the hash and TXEntry object for each relevant Tx hash.
         for(const auto &entry : le) {
            BinaryData searchHash(entry.getTxHash());
            const auto &itTX = txMap_.find(searchHash);
            if(itTX == txMap_.end()) {
               txHashSet_.insert(searchHash);
               txEntryHashSet_[searchHash] = bs::convertTXEntry(entry);
            }
         }
      }
      catch(std::exception& e) {
         if(logger_ != nullptr) {
            logger_->error("[AddressDetailsWidget::getTxData] Return data error " \
               "- {}", e.what());
         }
      }

      if(!txHashSet_.empty()) {
         armory_->getTXsByHash(txHashSet_, cbCollectTXs);
      }
   };
   delegate.getHistoryPage(0, cbLedger);  // ? should we use more than 0 pageId?
}

// Called when Armory has finished registering a wallet.
void AddressDetailsWidget::OnRefresh(std::vector<BinaryData> ids)
{
   // Make sure Armory is telling us about our wallet. Refreshes occur for
   // multiple item types.
   const auto &it = std::find(ids.begin(), ids.end(), dummyWalletID_);
   if (it == ids.end()) {
      return;
   }

   logger_->debug("[AddressDetailsWidget::OnRefresh] get refresh command");

   ui_->addressId->setText(addrVal_.display());
   // Once this callback runs, the data is safe to grab.
   const auto &cbBalance = [this](std::vector<uint64_t> balances) {
      double curBalance = dummyWallet_.GetTxBalance(balances[0]);
      ui_->balance->setText(QString::number(curBalance, 'f', 8));

      const auto &cbLedgerDelegate = [this](AsyncClient::LedgerDelegate delegate) {
         // UI changes in a non-main thread will trigger a crash. Invoke a new
         // thread to handle the received data. (UI changes happen eventually.)
         QMetaObject::invokeMethod(this, [this, &delegate] { getTxData(delegate); });
      };
      if(!armory_->getLedgerDelegateForAddress(dummyWallet_.GetWalletId(),
                                               addrVal_,
                                               cbLedgerDelegate)) {
         logger_->debug("[AddressDetailsWidget::OnRefresh] Failed to get "
                        "ledger delegate for wallet ID {} - address {}",
                        dummyWallet_.GetWalletId(), addrVal_.display<std::string>());
      }
   };
   dummyWallet_.UpdateBalanceFromDB(cbBalance);
}

// Clear all the fields.
void AddressDetailsWidget::clearFields() {
   ui_->addressId->clear();
   ui_->balance->clear();
   ui_->totalReceived->clear();
   ui_->totalSent->clear();
   ui_->treeAddressTransactions->clear();
}
