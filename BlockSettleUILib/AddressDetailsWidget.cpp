#include "AddressDetailsWidget.h"
#include "ui_AddressDetailsWidget.h"
#include <QDateTime>
#include <QtConcurrent/QtConcurrentRun>
#include "AddressVerificator.h"
#include "CheckRecipSigner.h"
#include "UiUtils.h"
#include "Wallets/SyncPlainWallet.h"
#include "Wallets/SyncWallet.h"
#include "Wallets/SyncWalletsManager.h"


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
void AddressDetailsWidget::init(const std::shared_ptr<ArmoryConnection> &armory
   , const std::shared_ptr<spdlog::logger> &inLogger
   , const std::shared_ptr<bs::sync::CCDataResolver> &resolver)
{
   armory_ = armory;
   logger_ = inLogger;
   ccResolver_ = resolver;

   act_ = make_unique<AddrDetailsACT>(this);
   act_->init(armory_.get());
}

void AddressDetailsWidget::setBSAuthAddrs(const std::unordered_set<std::string> &bsAuthAddrs)
{
   if (bsAuthAddrs.empty()) {
      return;
   }
   bsAuthAddrs_ = bsAuthAddrs;

   addrVerify_ = std::make_shared<AddressVerificator>(logger_, armory_
      , [this](const bs::Address &address, AddressVerificationState state) {
      authAddrStates_[address] = state;
      QMetaObject::invokeMethod(this, &AddressDetailsWidget::updateFields);
   });
   addrVerify_->SetBSAddressList(bsAuthAddrs);
}

// Set the address to be queried and perform initial setup.
void AddressDetailsWidget::setQueryAddr(const bs::Address &inAddrVal)
{
   // In case we've been here before, clear all the text.
   clear();

   {
      std::lock_guard<std::mutex> lock(mutex_);
      currentAddr_ = inAddrVal;
   }

   // Armory can't directly take an address and return all the required data.
   // Work around this by creating a dummy wallet, adding the explorer address,
   // registering the wallet, and getting the required data.
   const auto walletId = CryptoPRNG::generateRandom(8).toHexStr();
   const auto dummyWallet = std::make_shared<bs::sync::PlainWallet>(walletId
      , "temporary", "Dummy explorer wallet", nullptr, logger_);
   dummyWallet->addAddress(inAddrVal, {}, false);
   const auto regIds = dummyWallet->registerWallet(armory_, true);
   for (const auto &regId : regIds) {
      dummyWallets_[regId] = dummyWallet;
   }
   updateFields();
}

void AddressDetailsWidget::updateFields()
{
   if (!ccFound_.first.empty()) {
      ui_->addressId->setText(tr("%1 [Private Market: %2]")
         .arg(QString::fromStdString(currentAddr_.display()))
         .arg(QString::fromStdString(ccFound_.first)));
      if (balanceLoaded_) {
         ui_->totalReceived->setText(QString::number(totalReceived_ / ccFound_.second));
         ui_->totalSent->setText(QString::number(totalSpent_ / ccFound_.second));
         ui_->balance->setText(QString::number((totalReceived_ - totalSpent_) / ccFound_.second));

         for (int i = 0; i < ui_->treeAddressTransactions->topLevelItemCount(); ++i) {
            int64_t amt = ui_->treeAddressTransactions->topLevelItem(i)->data(colOutputAmt, Qt::UserRole).toLongLong();
            amt /= (int64_t)ccFound_.second;
            ui_->treeAddressTransactions->topLevelItem(i)->setData(colOutputAmt, Qt::DisplayRole
               , tr("%1 %2").arg(QString::number(amt)).arg(QString::fromStdString(ccFound_.first)));
         }
      }
   }
   else {
      const auto authIt = authAddrStates_.find(currentAddr_);
      if (bsAuthAddrs_.find(currentAddr_.display()) != bsAuthAddrs_.end()) {
         ui_->addressId->setText(tr("%1 [Authentication: BlockSettle funding address]")
            .arg(QString::fromStdString(currentAddr_.display())));
      }
      else if ((authIt != authAddrStates_.end()) && (authIt->second != AddressVerificationState::VerificationFailed)) {
         ui_->addressId->setText(tr("%1 [Authentication: %2]").arg(QString::fromStdString(currentAddr_.display()))
            .arg(QString::fromStdString(to_string(authIt->second))));
      }
      else {
         ui_->addressId->setText(QString::fromStdString(currentAddr_.display()));
      }
      if (balanceLoaded_) {
         ui_->totalReceived->setText(UiUtils::displayAmount(totalReceived_.load()));
         ui_->totalSent->setText(UiUtils::displayAmount(totalSpent_.load()));
         ui_->balance->setText(UiUtils::displayAmount(totalReceived_ - totalSpent_));
      }
   }
}

void AddressDetailsWidget::searchForCC()
{
   if (!ccFound_.first.empty()) {
      return;
   }
   for (const auto &txPair : txMap_) {
      for (const auto &ccSecurity : ccResolver_->securities()) {
         auto checker = std::make_shared<bs::TxAddressChecker>(
            ccResolver_->genesisAddrFor(ccSecurity), armory_);
         const auto &cbHasInput = [this, ccSecurity, checker](bool found) {
            if (!found || !ccFound_.first.empty()) {
               return;
            }
            ccFound_ = { ccSecurity, ccResolver_->lotSizeFor(ccSecurity) };
            QMetaObject::invokeMethod(this, &AddressDetailsWidget::updateFields);
         };
         if (!ccFound_.first.empty()) {
            break;
         }
         const auto nbSatoshis = ccResolver_->lotSizeFor(ccSecurity);
         if (((totalReceived_ % nbSatoshis) == 0) && ((totalSpent_ % nbSatoshis) == 0)) {
            checker->containsInputAddress(txPair.second, cbHasInput, nbSatoshis);
         }
      }
   }
}

void AddressDetailsWidget::searchForAuth()
{
   if (!addrVerify_) {
      return;
   }

   addrVerify_->addAddress(currentAddr_);
   addrVerify_->startAddressVerification();
}

// The function that gathers all the data to place in the UI.
void AddressDetailsWidget::loadTransactions()
{
   CustomTreeWidget *tree = ui_->treeAddressTransactions;
   tree->clear();

   uint64_t totCount = 0;

   QtConcurrent::run(this, &AddressDetailsWidget::searchForCC);
   QtConcurrent::run(this, &AddressDetailsWidget::searchForAuth);

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
      item->setData(colConfs, Qt::DisplayRole, armory_->getConfirmationsNumber(curTXEntry.second.blockNum));
      item->setText(colInputsNum, QString::number(tx.getNumTxIn()));
      item->setText(colOutputsNum, QString::number(tx.getNumTxOut()));
      item->setText(colOutputAmt, UiUtils::displayAmount(curTXEntry.second.value));
      item->setData(colOutputAmt, Qt::UserRole, (qlonglong)curTXEntry.second.value);
      item->setText(colFees, UiUtils::displayAmount(fees));
      item->setText(colFeePerByte, QString::number(std::nearbyint(feePerByte)));
      item->setText(colTxSize, QString::number(tx.getSize()));

      item->setTextAlignment(colOutputAmt, Qt::AlignRight);

      QFont font = item->font(colOutputAmt);
      font.setBold(true);
      item->setFont(colOutputAmt, font);

      // Check the total received or sent.
      if (curTXEntry.second.value > 0) {
         totalReceived_ += curTXEntry.second.value;
      }
      else {
         totalSpent_ -= curTXEntry.second.value; // Negative, so fake that out.
      }
      totCount++;

      setConfirmationColor(item);
      // disabled as per Scott's request
      //setOutputColor(item);
      tree->addTopLevelItem(item);
   }

   balanceLoaded_ = true;
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
   const auto &cbCollectPrevTXs = [this](const std::vector<Tx> &prevTxs) {
      for (const auto &prevTx : prevTxs) {
         txMap_[prevTx.getThisHash()] = prevTx;
      }
      // We're finally ready to display all the transactions.
      loadTransactions();
   };

   // Callback used to process Tx objects obtained from Armory. Used primarily
   // to obtain Tx entries for the TxIn objects we're checking.
   const auto &cbCollectTXs = [this, cbCollectPrevTXs](const std::vector<Tx> &txs) {
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
   const auto &cbLedger = [this, cbCollectTXs] (ReturnMessage<std::vector<ClientClasses::LedgerEntry>> entries) {
      auto result = std::make_shared<std::vector<ClientClasses::LedgerEntry>>();
      try {
         *result = entries.get();
      }
      catch (const std::exception &e) {
         logger_->error("[AddressDetailsWidget::getTxData] Return data error " \
            "- {}", e.what());
         return;
      }

      // Process entries on main thread because this callback is called from background
      QMetaObject::invokeMethod(this, [this, cbCollectTXs, result] {
         std::set<BinaryData> txHashSet; // Hashes assoc'd with a given address.

         // Get the hash and TXEntry object for each relevant Tx hash.
         for (const auto &entry : *result) {
            BinaryData searchHash(entry.getTxHash());
            const auto &itTX = txMap_.find(searchHash);
            if (itTX == txMap_.end()) {
               txHashSet.insert(searchHash);
               txEntryHashSet_[searchHash] = bs::TXEntry::fromLedgerEntry(entry);
            }
         }
         if (txHashSet.empty()) {
            logger_->info("[AddressDetailsWidget::getTxData] address participates in no TXs");
            cbCollectTXs({});
         } else {
            armory_->getTXsByHash(txHashSet, cbCollectTXs);
         }
      });
   };

   const auto &cbPageCnt = [this, delegate, cbLedger] (ReturnMessage<uint64_t> pageCnt) {
      try {
         uint64_t inPageCnt = pageCnt.get();
         for(uint64_t i = 0; i < inPageCnt; i++) {
            delegate->getHistoryPage(uint32_t(i), cbLedger);
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
void AddressDetailsWidget::refresh(const std::shared_ptr<bs::sync::PlainWallet> &wallet)
{
   logger_->debug("[{}] get refresh command for {}", __func__
                  , wallet->walletId());
   if (wallet->getUsedAddressCount() != 1) {
      logger_->debug("[{}] dummy wallet {} contains invalid amount of "
                     "addresses ({})", __func__, wallet->walletId()
                     , wallet->getUsedAddressCount());
      return;
   }

   // Process TX data for the "first" (i.e., only) address in the wallet.
   const auto &cbLedgerDelegate = [this](const std::shared_ptr<AsyncClient::LedgerDelegate> &delegate) {
      getTxData(delegate);
   };
   const auto addr = wallet->getUsedAddressList().at(0);
   if (!wallet->getLedgerDelegateForAddress(addr, cbLedgerDelegate)) {
      logger_->debug("[AddressDetailsWidget::refresh (cbBalance)] Failed to "
                     "get ledger delegate for wallet ID {} - address {}"
                     , wallet->walletId(), addr.display());
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
   for (const auto &dummyWallet : dummyWallets_) {
      dummyWallet.second->unregisterWallet();
   }
   balanceLoaded_ = false;
   totalReceived_ = 0;
   totalSpent_ = 0;
   dummyWallets_.clear();
   txMap_.clear();
   txEntryHashSet_.clear();
   ccFound_.first.clear();
   ccFound_.second = 0;
   authAddrStates_.clear();

   ui_->addressId->clear();
   ui_->treeAddressTransactions->clear();

   const auto &loading = tr("Loading...");
   ui_->balance->setText(loading);
   ui_->transactionCount->setText(loading);
   ui_->totalReceived->setText(loading);
   ui_->totalSent->setText(loading);
}
