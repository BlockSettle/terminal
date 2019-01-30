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

AddressDetailsWidget::~AddressDetailsWidget() = default;

// Initialize the widget and related widgets (block, address, Tx)
void AddressDetailsWidget::init(const std::shared_ptr<ArmoryConnection> &armory,
                                const std::shared_ptr<spdlog::logger> &inLogger)
{
   armory_ = armory;
   logger_ = inLogger;

   connect(armory_.get(), &ArmoryConnection::refresh, this
           , &AddressDetailsWidget::OnRefresh, Qt::QueuedConnection);
}

// Set the address to be queried and perform initial setup.
void AddressDetailsWidget::setQueryAddr(const bs::Address &inAddrVal)
{
   // In case we've been here before, clear all the text.
   clear();

   // Armory can't directly take an address and return all the required data.
   // Work around this by creating a dummy wallet, adding the explorer address,
   // registering the wallet, and getting the required data.
   const auto dummyWallet = std::make_shared<bs::PlainWallet>(logger_);
   dummyWallet->addAddress(inAddrVal);
   const auto regId = dummyWallet->RegisterWallet(armory_, true);
   dummyWallets_[regId] = dummyWallet;

   ui_->addressId->setText(inAddrVal.display());
}

// The function that gathers all the data to place in the UI.
void AddressDetailsWidget::loadTransactions()
{
   CustomTreeWidget *tree = ui_->treeAddressTransactions;
   tree->clear();

   uint64_t totSpent = 0;
   uint64_t totRcvd = 0;
   uint64_t totCount = 0;

   // Go through each TXEntry object and calculate all required UI data.
   for (const auto &curTXEntry : txEntryHashSet_) {
      QTreeWidgetItem *item = new QTreeWidgetItem(tree);

      auto tx = txMap_[curTXEntry.first];
      if (!tx.isInitialized()) {
         logger_->warn("[{}] TX with hash {} is not found or not inited"
            , __func__, curTXEntry.first.toHexStr(true));
         continue;
      }

      // Get fees & fee/byte by looping through the prev Tx set and calculating.
      uint64_t totIn = 0;
      for (size_t r = 0; r < tx.getNumTxIn(); ++r) {
         TxIn in = tx.getTxInCopy(r);
         OutPoint op = in.getOutPoint();
         const auto &prevTx = txMap_[op.getTxHash()];
         if (prevTx.isInitialized()) {
            TxOut prevOut = prevTx.getTxOutCopy(op.getTxOutIndex());
            totIn += prevOut.getValue();
         }
         else {
            logger_->warn("[{}] prev TX with hash {} is not found or is not"
               "initialized", __func__, op.getTxHash().toHexStr(true));
         }
      }
      uint64_t fees = totIn - tx.getSumOfOutputs();
      double feePerByte = (double)fees / (double)tx.getTxWeight();

      // Populate the transaction entries.
      item->setText(colDate,
                    UiUtils::displayDateTime(QDateTime::fromTime_t(curTXEntry.second.txTime)));
      item->setText(colTxId, // Flip Armory's TXID byte order: internal -> RPC
                    QString::fromStdString(curTXEntry.first.toHexStr(true)));
      item->setText(colConfs,
                    QString::number(armory_->getConfirmationsNumber(curTXEntry.second.blockNum)));
      item->setText(colInputsNum, QString::number(tx.getNumTxIn()));
      item->setText(colOutputsNum, QString::number(tx.getNumTxOut()));
      item->setText(colOutputAmt, UiUtils::displayAmount(curTXEntry.second.value));
      item->setText(colFees, UiUtils::displayAmount(fees));
      item->setText(colFeePerByte, QString::number(std::nearbyint(feePerByte)));
      item->setText(colTxSize, QString::number(tx.getSize()));

      // Check the total received or sent.
      if (curTXEntry.second.value > 0) {
         totRcvd += curTXEntry.second.value;
      }
      else {
         totSpent -= curTXEntry.second.value; // Negative, so fake that out.
      }
      totCount++;

      setConfirmationColor(item);
      // disabled as per Scott's request
      //setOutputColor(item);
      tree->addTopLevelItem(item);
   }

   // Set up the display for total rcv'd/spent.
   ui_->balance->setText(UiUtils::displayAmount(totRcvd - totSpent));
   ui_->totalReceived->setText(UiUtils::displayAmount(totRcvd));
   ui_->totalSent->setText(UiUtils::displayAmount(totSpent));
   ui_->transactionCount->setText(QString::number(totCount));

   tree->resizeColumns();
}

// This function sets the confirmation column to the correct color based
// on the number of confirmations that are set inside the column.
void AddressDetailsWidget::setConfirmationColor(QTreeWidgetItem *item)
{
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
void AddressDetailsWidget::setOutputColor(QTreeWidgetItem *item)
{
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

void AddressDetailsWidget::onTxClicked(QTreeWidgetItem *item, int column)
{
   // user has clicked the transaction column of the item so
   // send a signal to ExplorerWidget to open TransactionDetailsWidget
   if (column == colTxId) {
      emit(transactionClicked(item->text(colTxId)));
   }
}

// Used in refresh. The callback used when getting a ledger delegate (pages)
// from Armory. Processes the data as needed.
void AddressDetailsWidget::getTxData(const std::shared_ptr<AsyncClient::LedgerDelegate> &delegate)
{
   // The callback that handles previous Tx objects attached to the TxIn objects
   // and processes them. Once done, the UI can be changed.
   const auto &cbCollectPrevTXs = [this](std::vector<Tx> prevTxs) {
      for (const auto &prevTx : prevTxs) {
         txMap_[prevTx.getThisHash()] = prevTx;
      }
      // We're finally ready to display all the transactions.
      loadTransactions();
   };

   // Callback used to process Tx objects obtained from Armory. Used primarily
   // to obtain Tx entries for the TxIn objects we're checking.
   const auto &cbCollectTXs = [this, cbCollectPrevTXs](std::vector<Tx> txs) {
      std::set<BinaryData> prevTxHashSet; // Prev Tx hashes for an addr (fee calc).
      for (const auto &tx : txs) {
         const auto &prevTxHash = tx.getThisHash();
         txMap_[prevTxHash] = tx;

         // While here, we need to get the prev Tx with the UTXO being spent.
         // This is done so that we can calculate fees later.
         for (size_t i = 0; i < tx.getNumTxIn(); i++) {
            TxIn in = tx.getTxInCopy(i);
            OutPoint op = in.getOutPoint();
            const auto &itTX = txMap_.find(op.getTxHash());
            if (itTX == txMap_.end()) {
               prevTxHashSet.insert(op.getTxHash());
            }
         }
      }
      if (prevTxHashSet.empty()) {
         logger_->warn("[AddressDetailsWidget::getTxData] failed to get previous TXs");
         loadTransactions();
      }
      else {
         armory_->getTXsByHash(prevTxHashSet, cbCollectPrevTXs);
      }
   };

   // Callback to process ledger entries (pages) from the ledger delegate. Gets
   // Tx entries from Armory.
   const auto &cbLedger = [this, cbCollectTXs]
                          (ReturnMessage<std::vector<ClientClasses::LedgerEntry>> entries) {
      std::set<BinaryData> txHashSet; // Hashes assoc'd with a given address.
      try {
         auto le = entries.get();
         // Get the hash and TXEntry object for each relevant Tx hash.
         for (const auto &entry : le) {
            BinaryData searchHash(entry.getTxHash());
            const auto &itTX = txMap_.find(searchHash);
            if (itTX == txMap_.end()) {
               txHashSet.insert(searchHash);
               txEntryHashSet_[searchHash] = bs::convertTXEntry(entry);
            }
         }
      }
      catch (const std::exception &e) {
         if (logger_ != nullptr) {
            logger_->error("[AddressDetailsWidget::getTxData] Return data error " \
               "- {}", e.what());
         }
      }

      if (txHashSet.empty()) {
         logger_->info("[AddressDetailsWidget::getTxData] address participates in no TXs");
      }
      else {
         armory_->getTXsByHash(txHashSet, cbCollectTXs);
      }
   };

   const auto &cbPageCnt = [this, delegate, cbLedger]
                           (ReturnMessage<uint64_t> pageCnt)->void {
      try {
         const auto &inPageCnt = pageCnt.get();
         for(uint64_t i = 0; i < inPageCnt; i++) {
            delegate->getHistoryPage(i, cbLedger);
         }
      }
      catch (const std::exception &e) {
         logger_->error("[AddressDetailsWidget::getTxData] Return data " \
            "error (getPageCount) - {}", e.what());
      }
   };
   delegate->getPageCount(cbPageCnt);
}

// Function that grabs the TX data for the address. Used in callback.
void AddressDetailsWidget::refresh(const std::shared_ptr<bs::PlainWallet> &wallet)
{
   logger_->debug("[{}] get refresh command for {}", __func__
                  , wallet->GetWalletId());
   if (wallet->GetUsedAddressCount() != 1) {
      logger_->debug("[{}}] dummy wallet {} contains invalid amount of "
                     "addresses ({})", __func__, wallet->GetWalletId()
                     , wallet->GetUsedAddressCount());
      return;
   }

   // Process TX data for the "first" (i.e., only) address in the wallet.
   const auto &cbLedgerDelegate = [this](const std::shared_ptr<AsyncClient::LedgerDelegate> &delegate) {
      getTxData(delegate);
   };
   const auto addr = wallet->GetUsedAddressList()[0];
   if(!armory_->getLedgerDelegateForAddress(wallet->GetWalletId(), addr
      , cbLedgerDelegate)) {
      logger_->debug("[AddressDetailsWidget::refresh (cbBalance)] Failed to "
                     "get ledger delegate for wallet ID {} - address {}"
                     , wallet->GetWalletId(), addr.display<std::string>());
   }
}

// Called when Armory has finished registering a wallet. Kicks off the function
// that grabs the address's TX data.
void AddressDetailsWidget::OnRefresh(std::vector<BinaryData> ids)
{
   // Make sure Armory is telling us about our wallet. Refreshes occur for
   // multiple item types.
   for (const auto &id : ids) {
      const auto &itWallet = dummyWallets_.find(id.toBinStr());
      if (itWallet != dummyWallets_.end()) {
         refresh(itWallet->second);
      }
   }
}

// Clear out all address details.
void AddressDetailsWidget::clear()
{
   for (const auto &dummyWallet : dummyWallets_) {
      dummyWallet.second->UnregisterWallet();
   }
   dummyWallets_.clear();
   txMap_.clear();
   txEntryHashSet_.clear();

   ui_->addressId->clear();
   ui_->treeAddressTransactions->clear();

   const auto &loading = tr("Loading...");
   ui_->balance->setText(loading);
   ui_->transactionCount->setText(loading);
   ui_->totalReceived->setText(loading);
   ui_->totalSent->setText(loading);
}
