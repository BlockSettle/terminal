/*

***********************************************************************************
* Copyright (C) 2018 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "AddressDetailsWidget.h"
#include "ui_AddressDetailsWidget.h"

#include <QDateTime>
#include <spdlog/spdlog.h>
#include "CheckRecipSigner.h"
#include "UiUtils.h"
#include "Wallets/SyncPlainWallet.h"
#include "Wallets/SyncWallet.h"
#include "Wallets/SyncWalletsManager.h"

namespace {

   const uint64_t kAuthAddrValue = 1000;

}


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

void AddressDetailsWidget::init(const std::shared_ptr<spdlog::logger> &logger)
{
   logger_ = logger;
}

// Set the address to be queried and perform initial setup.
void AddressDetailsWidget::setQueryAddr(const bs::Address &inAddrVal)
{
   // In case we've been here before, clear all the text.
   clear();

   {
      std::lock_guard<std::mutex> lock(mutex_);
      currentAddr_ = inAddrVal;
      currentAddrStr_ = currentAddr_.display();
   }

   // Armory can't directly take an address and return all the required data.
   // Work around this by creating a dummy wallet, adding the explorer address,
   // registering the wallet, and getting the required data.
   ui_->addressId->setText(QString::fromStdString(currentAddrStr_));
   emit needAddressHistory(inAddrVal);
   updateFields();
}

void AddressDetailsWidget::updateFields()
{
   ui_->addressId->setText(QString::fromStdString(currentAddrStr_));
}

// The function that gathers all the data to place in the UI.
void AddressDetailsWidget::loadTransactions()
{
   CustomTreeWidget *tree = ui_->treeAddressTransactions;
   tree->clear();

   uint64_t totCount = 0;

   // Go through each TXEntry object and calculate all required UI data.
   for (const auto &curTXEntry : txEntryHashSet_) {
      QTreeWidgetItem *item = new QTreeWidgetItem(tree);

      auto tx = txMap_[curTXEntry.first];
      if (!tx || !tx->isInitialized()) {
         SPDLOG_LOGGER_WARN(logger_, "TX with hash {} is not found or not inited"
            , curTXEntry.first.toHexStr(true));
         continue;
      }

      // Get fees & fee/byte by looping through the prev Tx set and calculating.
      uint64_t totIn = 0;
      for (size_t r = 0; r < tx->getNumTxIn(); ++r) {
         TxIn in = tx->getTxInCopy(r);
         OutPoint op = in.getOutPoint();
         const auto &prevTx = txMap_[op.getTxHash()];
         if (prevTx && prevTx->isInitialized()) {
            TxOut prevOut = prevTx->getTxOutCopy(op.getTxOutIndex());
            totIn += prevOut.getValue();
         }
         else {
            SPDLOG_LOGGER_WARN(logger_, "prev TX with hash {} is not found or is notinitialized"
               , op.getTxHash().toHexStr(true));
         }
      }
      uint64_t fees = totIn - tx->getSumOfOutputs();
      double feePerByte = (double)fees / (double)tx->getTxWeight();

      // Populate the transaction entries.
      item->setText(colDate,
                    UiUtils::displayDateTime(QDateTime::fromTime_t(curTXEntry.second.txTime)));
      item->setText(colTxId, // Flip Armory's TXID byte order: internal -> RPC
                    QString::fromStdString(curTXEntry.first.toHexStr(true)));
      item->setText(colInputsNum, QString::number(tx->getNumTxIn()));
      item->setText(colOutputsNum, QString::number(tx->getNumTxOut()));
      item->setText(colFees, UiUtils::displayAmount(fees));
      item->setText(colFeePerByte, QString::number(std::nearbyint(feePerByte)));
      item->setText(colTxSize, QString::number(tx->getSize()));

      item->setText(colOutputAmt, UiUtils::displayAmount(curTXEntry.second.value));
      item->setTextAlignment(colOutputAmt, Qt::AlignRight);

      QFont font = item->font(colOutputAmt);
      font.setBold(true);
      item->setFont(colOutputAmt, font);

      totCount++;

      setConfirmationColor(item);
      tree->addTopLevelItem(item);
   }

   ui_->totalReceived->setText(UiUtils::displayAmount(totalReceived_));
   ui_->totalSent->setText(UiUtils::displayAmount(totalSpent_));
   ui_->balance->setText(UiUtils::displayAmount(totalReceived_ - totalSpent_));
   emit finished();

   // Set up the display for total rcv'd/spent.
   ui_->transactionCount->setText(QString::number(totCount));

   updateFields();

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
   const auto &cbCollectPrevTXs = [this]
      (const AsyncClient::TxBatchResult &prevTxs, std::exception_ptr)
   {
      txMap_.insert(prevTxs.cbegin(), prevTxs.cend());
      // We're finally ready to display all the transactions.
      loadTransactions();
   };

   // Callback used to process Tx objects obtained from Armory. Used primarily
   // to obtain Tx entries for the TxIn objects we're checking.
   const auto &cbCollectTXs = [this, cbCollectPrevTXs]
      (const AsyncClient::TxBatchResult &txs, std::exception_ptr)
   {
      std::set<BinaryData> prevTxHashSet; // Prev Tx hashes for an addr (fee calc).
      for (const auto &tx : txs) {
         if (!tx.second) {
            continue;
         }
         txMap_[tx.first] = tx.second;

         // While here, we need to get the prev Tx with the UTXO being spent.
         // This is done so that we can calculate fees later.
         for (size_t i = 0; i < tx.second->getNumTxIn(); i++) {
            TxIn in = tx.second->getTxInCopy(i);
            OutPoint op = in.getOutPoint();
            const auto &itTX = txMap_.find(op.getTxHash());
            if (itTX == txMap_.end()) {
               prevTxHashSet.insert(op.getTxHash());
            }
         }
      }
      if (prevTxHashSet.empty()) {
         SPDLOG_LOGGER_WARN(logger_, "failed to get previous TXs");
         loadTransactions();
      }
   };
}

// Function that grabs the TX data for the address. Used in callback.
void AddressDetailsWidget::refresh(const std::shared_ptr<bs::sync::PlainWallet> &wallet)
{
   SPDLOG_LOGGER_DEBUG(logger_, "get refresh command for {}", wallet->walletId());
   if (wallet->getUsedAddressCount() != 1) {
      SPDLOG_LOGGER_DEBUG(logger_, "dummy wallet {} contains invalid amount of addresses ({})"
         , wallet->walletId(), wallet->getUsedAddressCount());
      return;
   }
}

// Called when Armory has finished registering a wallet. Kicks off the function
// that grabs the address's TX data.
void AddressDetailsWidget::OnRefresh(std::vector<BinaryData> ids, bool online)
{
   if (!online) {
      return;
   }
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
   totalReceived_ = 0;
   totalSpent_ = 0;
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

void AddressDetailsWidget::onNewBlock(unsigned int blockNum)
{
   topBlock_ = blockNum;
   //TODO: update conf counts
}

void AddressDetailsWidget::onAddressHistory(const bs::Address& addr, uint32_t curBlock, const std::vector<bs::TXEntry>& entries)
{
   if (addr != currentAddr_) {
      return;  // address has probably changed
   }
   topBlock_ = curBlock;
   txEntryHashSet_.clear();

   // Get the hash and TXEntry object for each relevant Tx hash.
   std::vector<bs::sync::TXWallet> txDetRequest;
   for (const auto& entry : entries) {
      const auto& itTX = txMap_.find(entry.txHash);
      if (itTX == txMap_.end()) {
         txDetRequest.push_back({ entry.txHash, {}, entry.value });
         txEntryHashSet_[entry.txHash] = entry;
      }
   }
   if (txDetRequest.empty()) {
      SPDLOG_LOGGER_INFO(logger_, "address {} participates in no TXs", currentAddrStr_);
      onTXDetails({});
   }
   else {
      emit needTXDetails(txDetRequest, false, currentAddr_);
   }
}

void AddressDetailsWidget::onTXDetails(const std::vector<bs::sync::TXWalletDetails>& txDet)
{
   for (const auto& tx : txDet) {
      if (!tx.txHash.empty() && (txEntryHashSet_.find(tx.txHash) == txEntryHashSet_.end())) {
         return;    // not our TX details
      }
   }
   CustomTreeWidget* tree = ui_->treeAddressTransactions;
   tree->clear();

   unsigned int totCount = 0;

   // Go through each TXEntry object and calculate all required UI data.
   for (const auto& tx : txDet) {
      QTreeWidgetItem* item = new QTreeWidgetItem(tree);
      // Get fees & fee/byte by looping through the prev Tx set and calculating.
      uint64_t fees = 0;
      for (const auto& inAddr : tx.inputAddresses) {
         fees += inAddr.value;
      }
      for (const auto& outAddr : tx.outputAddresses) {
         fees -= outAddr.value;
      }
      double feePerByte = (double)fees / (double)tx.tx.getTxWeight();

      if (!tx.txHash.empty()) {
         const auto& itEntry = txEntryHashSet_.find(tx.txHash);
         if (itEntry == txEntryHashSet_.end()) {
            logger_->error("[{}] can't find TXEntry for {}", __func__, tx.txHash.toHexStr(true));
            continue;
         }

         // Populate the transaction entries.
         item->setText(colDate,
            UiUtils::displayDateTime(QDateTime::fromTime_t(itEntry->second.txTime)));
         item->setText(colTxId, // Flip Armory's TXID byte order: internal -> RPC
            QString::fromStdString(tx.txHash.toHexStr(true)));
         item->setData(colConfs, Qt::DisplayRole, itEntry->second.nbConf);
         item->setText(colInputsNum, QString::number(tx.tx.getNumTxIn()));
         item->setText(colOutputsNum, QString::number(tx.tx.getNumTxOut()));
         item->setText(colFees, UiUtils::displayAmount(fees));
         item->setText(colFeePerByte, QString::number(std::nearbyint(feePerByte)));
         item->setText(colTxSize, QString::number(tx.tx.getSize()));
         item->setText(colOutputAmt, UiUtils::displayAmount(itEntry->second.value));
         item->setTextAlignment(colOutputAmt, Qt::AlignRight);

         QFont font = item->font(colOutputAmt);
         font.setBold(true);
         item->setFont(colOutputAmt, font);

         if (!tx.isValid) {
            // Mark invalid transactions
            item->setTextColor(colOutputAmt, Qt::red);
         }

         // Check the total received or sent.
         if (itEntry->second.value > 0) {
            totalReceived_ += itEntry->second.value;
         }
         else {
            totalSpent_ -= itEntry->second.value; // Negative, so fake that out.
         }
         totCount++;
      }
      else if (!tx.comment.empty()) {
         item->setTextColor(colTxId, Qt::red);
         item->setText(colTxId, QString::fromStdString(tx.comment));
      }

      setConfirmationColor(item);
      tree->addTopLevelItem(item);
   }

   ui_->totalReceived->setText(UiUtils::displayAmount(totalReceived_));
   ui_->totalSent->setText(UiUtils::displayAmount(totalSpent_));
   ui_->balance->setText(UiUtils::displayAmount(totalReceived_ - totalSpent_));

   emit finished();

   // Set up the display for total rcv'd/spent.
   ui_->transactionCount->setText(QString::number(totCount));

   tree->resizeColumns();
}
