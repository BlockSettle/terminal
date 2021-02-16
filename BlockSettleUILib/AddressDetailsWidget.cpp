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
#include "AddressVerificator.h"
#include "CheckRecipSigner.h"
#include "ColoredCoinLogic.h"
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

void AddressDetailsWidget::setBSAuthAddrs(const std::unordered_set<std::string> &bsAuthAddrs)
{
   if (bsAuthAddrs.empty()) {
      return;
   }
   bsAuthAddrs_ = bsAuthAddrs;
}

// Set the address to be queried and perform initial setup.
void AddressDetailsWidget::setQueryAddr(const bs::Address &inAddrVal)
{
   // In case we've been here before, clear all the text.
   clear();

   currentAddr_ = inAddrVal;
   currentAddrStr_ = currentAddr_.display();

   ui_->addressId->setText(QString::fromStdString(currentAddrStr_));
   emit needAddressHistory(inAddrVal);
   updateFields();
}

void AddressDetailsWidget::updateFields()
{
   if (!ccFound_.security.empty()) {
      if (!ccFound_.isGenesisAddr) {
         ui_->addressId->setText(tr("%1 [Private Market: %2]")
            .arg(QString::fromStdString(currentAddrStr_))
            .arg(QString::fromStdString(ccFound_.security)));
      } else {
         ui_->addressId->setText(tr("%1 [Private Market: Genesis Address %2]")
            .arg(QString::fromStdString(currentAddrStr_))
            .arg(QString::fromStdString(ccFound_.security)));
      }
      return;
   }

   if (bsAuthAddrs_.find(currentAddrStr_) != bsAuthAddrs_.end()) {
      ui_->addressId->setText(tr("%1 [Authentication: BlockSettle funding address]")
         .arg(QString::fromStdString(currentAddrStr_)));
      return;
   }

   if (isAuthAddr_) {
      const auto authIt = authAddrStates_.find(currentAddr_);
      if ((authIt != authAddrStates_.end()) && (authIt->second != AddressVerificationState::VerificationFailed)) {
         ui_->addressId->setText(tr("%1 [Authentication: %2]").arg(QString::fromStdString(currentAddrStr_))
            .arg(QString::fromStdString(to_string(authIt->second))));
      }
      return;
   }

   ui_->addressId->setText(QString::fromStdString(currentAddrStr_));
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

// Clear out all address details.
void AddressDetailsWidget::clear()
{
   totalReceived_ = 0;
   totalSpent_ = 0;
   txEntryHashSet_.clear();
   ccFound_ = {};
   isAuthAddr_ = false;
   authAddrStates_.clear();

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
      if (txEntryHashSet_.find(entry.txHash) == txEntryHashSet_.end()) {
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
      if (txEntryHashSet_.find(tx.txHash) == txEntryHashSet_.end()) {
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
