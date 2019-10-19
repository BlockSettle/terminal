#include "TransactionDetailsWidget.h"
#include "ui_TransactionDetailsWidget.h"
#include "BTCNumericTypes.h"
#include "BlockObj.h"
#include "CheckRecipSigner.h"
#include "UiUtils.h"
#include "Wallets/SyncWallet.h"
#include "Wallets/SyncWalletsManager.h"

#include <memory>
#include <QToolTip>

Q_DECLARE_METATYPE(Tx);

BinaryData BinaryTXID::getRPCTXID()
{
   BinaryData retVal;
   if (txidIsRPC_ == true) {
      retVal = txid_;
   }
   else {
      retVal = txid_.swapEndian();
   }

   return retVal;
}

BinaryData BinaryTXID::getInternalTXID()
{
   BinaryData retVal;
   if (txidIsRPC_ == false) {
      retVal = txid_;
   }
   else {
      retVal = txid_.swapEndian();
   }

   return retVal;
}

bool BinaryTXID::operator==(const BinaryTXID& inTXID) const
{
   return (txidIsRPC_ == inTXID.getTXIDIsRPC()) &&
          (txid_ == inTXID.getRawTXID());
}

bool BinaryTXID::operator<(const BinaryTXID& inTXID) const
{
   return txid_ < inTXID.getRawTXID();
}

bool BinaryTXID::operator>(const BinaryTXID& inTXID) const
{
   return txid_ > inTXID.getRawTXID();
}


TransactionDetailsWidget::TransactionDetailsWidget(QWidget *parent) :
    QWidget(parent),
   ui_(new Ui::TransactionDetailsWidget)
{
   ui_->setupUi(this);

   // setting up a tooltip that pops up immediately when mouse hovers over it
   QIcon btcIcon(QLatin1String(":/resources/notification_info.png"));
   ui_->labelTxPopup->setPixmap(btcIcon.pixmap(13, 13));
   ui_->labelTxPopup->setMouseTracking(true);
   ui_->labelTxPopup->toolTip_ = tr("The Transaction ID (TXID) uses RPC byte "
                                    "order. It will match the RPC output from "
                                    "Bitcoin Core, along with the byte order "
                                    "from the BlockSettle Terminal.");

   // set the address column to have hand cursor
   ui_->treeInput->handCursorColumns_.append(colAddressId);
   ui_->treeOutput->handCursorColumns_.append(colAddressId);
   // allow address columns to be copied to clipboard with right click
   ui_->treeInput->copyToClipboardColumns_.append(colAddressId);
   ui_->treeOutput->copyToClipboardColumns_.append(colAddressId);

   connect(ui_->treeInput, &QTreeWidget::itemClicked,
      this, &TransactionDetailsWidget::onAddressClicked);
   connect(ui_->treeOutput, &QTreeWidget::itemClicked,
      this, &TransactionDetailsWidget::onAddressClicked);
}

TransactionDetailsWidget::~TransactionDetailsWidget() = default;

// Initialize the widget and related widgets (block, address, Tx)
void TransactionDetailsWidget::init(
   const std::shared_ptr<ArmoryConnection> &armory
   , const std::shared_ptr<spdlog::logger> &inLogger
   , const std::shared_ptr<bs::sync::WalletsManager> &walletsMgr
   , const std::shared_ptr<bs::sync::CCDataResolver> &resolver)
{
   armoryPtr_ = armory;
   logger_ = inLogger;
   walletsMgr_ = walletsMgr;
   ccResolver_ = resolver;
   act_ = make_unique<TxDetailsACT>(this);
   act_->init(armoryPtr_.get());
}

// This function uses getTxByHash() to retrieve info about transaction. The
// incoming TXID must be in RPC order, not internal order.
void TransactionDetailsWidget::populateTransactionWidget(BinaryTXID rpcTXID,
                                                         const bool& firstPass)
{
   if (!armoryPtr_) {
      if (logger_) {
         logger_->error("[{}] Armory is not initialized.", __func__);
      }
      return;
   }

   // In case we've been here earlier, clear all the text.
   if (firstPass) {
      clear();
   }
   // get the transaction data from armory
   std::string txidStr = rpcTXID.getRPCTXID().toHexStr();
   const auto &cbTX = [this, txidStr](const Tx &tx) {
      if (!tx.isInitialized()) {
         if (logger_) {
            logger_->error("[{}] TXID {} is not initialized.", __func__, txidStr);
         }
         ui_->tranID->setText(tr("%1 (load failed)").arg(QString::fromStdString(txidStr)));
         emit finished();
         return;
      }

      processTxData(tx);
   };

   // The TXID passed to Armory *must* be in internal order!
   if (!armoryPtr_->getTxByHash(rpcTXID.getInternalTXID(), cbTX)) {
      if (logger_) {
         logger_->error("[{}] - Failed to get TXID {}.", __func__, txidStr);
      }
   }
}

// Used in callback to process the Tx object returned by Armory.
void TransactionDetailsWidget::processTxData(Tx tx)
{
   // Save Tx and the prev Tx entries (get input amounts & such)
   curTx_ = tx;
   ui_->tranID->setText(QString::fromStdString(curTx_.getThisHash().toHexStr(true)));

   // Get each Tx object associated with the Tx's TxIn object. Needed to calc
   // the fees.
   const auto &cbProcessTX = [this]
      (const std::vector<Tx> &prevTxs, std::exception_ptr)
   {
      for (const auto &prevTx : prevTxs) {
         BinaryTXID intPrevTXHash(prevTx.getThisHash(), false);
         prevTxMap_[intPrevTXHash] = prevTx;
      }

      // We're ready to display all the transaction-related data in the UI.
      setTxGUIValues();
   };

   std::set<BinaryData> prevTxHashSet; // A Tx's associated prev Tx hashes.
   // While here, we need to get the prev Tx with the UTXO being spent.
   // This is done so that we can calculate fees later.
   for (size_t i = 0; i < tx.getNumTxIn(); i++) {
      TxIn in = tx.getTxInCopy(i);
      OutPoint op = in.getOutPoint();
      BinaryTXID intPrevTXID(op.getTxHash(), false);
      const auto &itTX = prevTxMap_.find(intPrevTXID);
      if (itTX == prevTxMap_.end()) {
         prevTxHashSet.insert(intPrevTXID.getInternalTXID());
      }
   }

   // Get the TxIn-associated Tx objects from Armory.
   if (prevTxHashSet.empty()) {
      setTxGUIValues();
   }
   else {
      armoryPtr_->getTXsByHash(prevTxHashSet, cbProcessTX);
   }
}

// NB: Don't use ClientClasses::BlockHeader. It has parsing capabilities but
// it's meant to be an internal Armory class, touching things like the DB. Just
// parse the raw data header here.
void TransactionDetailsWidget::getHeaderData(const BinaryData& inHeader)
{
   if (inHeader.getSize() != 80) {
      if (logger_) {
         logger_->error("[{}] Header is not the correct size - size = {}"
            , __func__, inHeader.getSize());
      }
         return;
   }

   // FIX - May want to rethink where the data is saved.
/*   curTxVersion = READ_UINT32_LE(inHeader.getPtr());
   curTxPrevHash = BinaryData(inHeader.getPtr() + 4, 32);
   curTxMerkleRoot = BinaryData(inHeader.getPtr() + 36, 32);
   curTxTimestamp = READ_UINT32_LE(inHeader.getPtr() + 68);
   curTxDifficulty = BinaryData(inHeader.getPtr() + 72, 4);
   curTxNonce = READ_UINT32_LE(inHeader.getPtr() + 76);*/
}

// The function that will actually populate the GUI with TX data.
void TransactionDetailsWidget::setTxGUIValues()
{
   // Get Tx header data. NOT USED FOR NOW.
//   BinaryData txHdr(curTx_.getPtr(), 80);
//   getHeaderData(txHdr);

   // Get fees & fee/byte by looping through the prev Tx set and calculating.
   uint64_t totIn = 0;
   for (size_t r = 0; r < curTx_.getNumTxIn(); ++r) {
      TxIn in = curTx_.getTxInCopy(r);
      OutPoint op = in.getOutPoint();
      BinaryTXID intPrevTXID(op.getTxHash(), false);
      const auto &prevTx = prevTxMap_[intPrevTXID];
      if (prevTx.isInitialized()) {
         TxOut prevOut = prevTx.getTxOutCopy(op.getTxOutIndex());
         totIn += prevOut.getValue();
      }
   }

   emit finished();

   uint64_t fees = totIn - curTx_.getSumOfOutputs();
   float feePerByte = (float)fees / (float)curTx_.getTxWeight();

   // NB: Certain data (timestamp, height, and # of confs) can't be obtained
   // from the Tx object. For now, we're leaving placeholders until a solution
   // can be found. In theory, the timestamp can be obtained from the header.
   // The header data retrieved right now seems to be inaccurate, so we're not
   // using that right now.

   // Populate the GUI fields. (NOTE: Armory's getTxWeight() call needs to be
   // relabeled getVirtSize().)
   // Output TXID in RPC byte order by flipping TXID bytes rcv'd by Armory (internal
   // order).
//   ui_->tranDate->setText(UiUtils::displayDateTime(QDateTime::fromTime_t(curTxTimestamp)));

   ui_->tranNumInputs->setText(QString::number(curTx_.getNumTxIn()));
   ui_->tranNumOutputs->setText(QString::number(curTx_.getNumTxOut()));
   ui_->tranInput->setText(UiUtils::displayAmount(totIn));
   ui_->tranOutput->setText(UiUtils::displayAmount(curTx_.getSumOfOutputs()));
   ui_->tranFees->setText(UiUtils::displayAmount(fees));
   ui_->tranFeePerByte->setText(QString::number(nearbyint(feePerByte)));
   ui_->tranSize->setText(QString::number(curTx_.getTxWeight()));

   loadInputs();
}

void TransactionDetailsWidget::onNewBlock(unsigned int)
{
   if (curTx_.isInitialized()) {
      populateTransactionWidget({ curTx_.getThisHash(), true }, false);
   }
}

// Load the input and output windows.
void TransactionDetailsWidget::loadInputs()
{
   loadTreeIn(ui_->treeInput);
   loadTreeOut(ui_->treeOutput);
}

void TransactionDetailsWidget::updateTreeCC(QTreeWidget *tree
   , const std::string &cc, uint64_t lotSize)
{
   for (int i = 0; i < tree->topLevelItemCount(); ++i) {
      auto item = tree->topLevelItem(i);
      const uint64_t amt = item->data(1, Qt::UserRole).toULongLong();
      if (amt && ((amt % lotSize) == 0)) {
         item->setData(1, Qt::DisplayRole, QString::number(amt / lotSize));
         const auto addrWallet = item->data(2, Qt::DisplayRole).toString();
         if (addrWallet.isEmpty()) {
            item->setData(2, Qt::DisplayRole, QString::fromStdString(cc));
         }
         for (int j = 0; j < item->childCount(); ++j) {
            auto outItem = item->child(j);
            const uint64_t outAmt = outItem->data(1, Qt::UserRole).toULongLong();
            outItem->setData(1, Qt::DisplayRole, QString::number(outAmt / lotSize));
            outItem->setData(2, Qt::DisplayRole, QString::fromStdString(cc));
         }
      }
   }
}

void TransactionDetailsWidget::checkTxForCC(const Tx &tx, QTreeWidget *treeWidget)
{
   for (const auto &cc : ccResolver_->securities()) {
      const auto &genesisAddr = ccResolver_->genesisAddrFor(cc);
      auto txChecker = std::make_shared<bs::TxAddressChecker>(genesisAddr, armoryPtr_);
      const auto &cbHasGA = [this, txChecker, treeWidget, cc](bool found) {
         if (!found) {
            return;
         }
         QMetaObject::invokeMethod(this, [this, treeWidget, cc] {
            updateTreeCC(treeWidget, cc, ccResolver_->lotSizeFor(cc));
         });
      };
      txChecker->containsInputAddress(tx, cbHasGA, ccResolver_->lotSizeFor(cc));
   }
}

void TransactionDetailsWidget::updateCCInputs()
{
   const auto &cbUpdateCCInput = [this](const BinaryData &txHash
      , const bs::network::CCSecurityDef &ccDef) {
   };

   for (size_t i = 0; i < curTx_.getNumTxIn(); ++i) {
      const OutPoint op = curTx_.getTxInCopy(i).getOutPoint();
      const auto &prevTx = prevTxMap_[op.getTxHash()];
      checkTxForCC(prevTx, ui_->treeInput);
   }
}

// Input widget population.
void TransactionDetailsWidget::loadTreeIn(CustomTreeWidget *tree)
{
   tree->clear();

   std::map<BinaryTXID, unsigned int> hashCounts;
   for (size_t i = 0; i < curTx_.getNumTxIn(); i++) {
      TxOut prevOut;
      const OutPoint op = curTx_.getTxInCopy(i).getOutPoint();
      const BinaryTXID txID(op.getTxHash(), false);
      hashCounts[txID]++;
   }

   // here's the code to add data to the Input tree.
   for (size_t i = 0; i < curTx_.getNumTxIn(); i++) {
      TxOut prevOut;
      const TxIn in = curTx_.getTxInCopy(i);
      const OutPoint op = in.getOutPoint();
      const BinaryTXID intPrevTXID(op.getTxHash(), false);
      const auto &prevTx = prevTxMap_[intPrevTXID];
      if (prevTx.isInitialized()) {
         prevOut = prevTx.getTxOutCopy(op.getTxOutIndex());
      }
      auto txType = prevOut.getScriptType();
      const auto outAddr = bs::Address::fromTxOut(prevOut);
      const auto addressWallet = walletsMgr_->getWalletByAddress(outAddr);
      QString addrStr;
      const QString walletName = addressWallet ? QString::fromStdString(addressWallet->name()) : QString();

      // For now, don't display any data if the TxOut is non-std. Displaying a
      // hex version of the script is one thing that could be done. This needs
      // to be discussed before implementing. Non-std could mean many things.
      if (txType == TXOUT_SCRIPT_NONSTANDARD) {
         addrStr = tr("<Non-Standard>");
      }
      else {
         addrStr = QString::fromStdString(outAddr.display());
      }

      // create a top level item using type, address, amount, wallet values
      addItem(tree, addrStr, prevOut.getValue(), walletName, prevTx.getThisHash()
         , (hashCounts[intPrevTXID] > 1) ? op.getTxOutIndex() : -1);
   }
   tree->resizeColumns();

   updateCCInputs();
}

// Output widget population.
void TransactionDetailsWidget::loadTreeOut(CustomTreeWidget *tree)
{
   tree->clear();

   // here's the code to add data to the Input tree.
   for (size_t i = 0; i < curTx_.getNumTxOut(); i++) {
      TxOut txOut = curTx_.getTxOutCopy(i);
      auto txType = txOut.getScriptType();
      const auto outAddr = bs::Address::fromTxOut(txOut);
      const auto addressWallet = walletsMgr_->getWalletByAddress(outAddr);
      QString addrStr;
      const QString walletName = addressWallet ? QString::fromStdString(addressWallet->name()) : QString();

      // For now, don't display any data if the TxOut is OP_RETURN or non-std.
      // Displaying a hex version of the script is one thing that could be done.
      // This needs to be discussed before implementing. OP_RETURN isn't too bad
      // (80 bytes max) but non-std could mean just about anything.
      if (txType == TXOUT_SCRIPT_OPRETURN) {
         addrStr = tr("<OP_RETURN>");
      }
      else if (txType == TXOUT_SCRIPT_NONSTANDARD) {
         addrStr = tr("<Non-Standard>");
      }
      else {
         addrStr = QString::fromStdString(outAddr.display());
      }

      addItem(tree, addrStr, txOut.getValue(), walletName, txOut.getScript());

      // add the item to the tree
   }
   tree->resizeColumns();

   checkTxForCC(curTx_, ui_->treeOutput);
}

void TransactionDetailsWidget::addItem(QTreeWidget *tree, const QString &address
   , const uint64_t amount, const QString &wallet, const BinaryData &txHash
   , const int txIndex)
{
   const bool specialAddr = address.startsWith(QLatin1Char('<'));
   const bool isOutput = (tree == ui_->treeOutput);
   auto &itemsMap = isOutput ? outputItems_ : inputItems_;
   auto item = itemsMap[address];
   if (!item || specialAddr) {
      QStringList items;
      const auto amountStr = UiUtils::displayAmount(amount);
      items << address << amountStr << wallet;
      item = new QTreeWidgetItem(items);
      item->setData(0, Qt::UserRole, isOutput);
      item->setData(1, Qt::UserRole, (qulonglong)amount);
      tree->addTopLevelItem(item);
      item->setExpanded(true);
      if (!specialAddr) {
         itemsMap[address] = item;
      }
   }
   else {
      uint64_t prevAmount = item->data(1, Qt::UserRole).toULongLong();
      prevAmount += amount;
      item->setData(1, Qt::UserRole, (qulonglong)prevAmount);
      item->setData(1, Qt::DisplayRole, UiUtils::displayAmount(prevAmount));
   }
   if (!specialAddr) {
      auto txHashStr = QString::fromStdString(txHash.toHexStr(!isOutput));
      if (txIndex >= 0) {
         txHashStr += QLatin1String(":") + QString::number(txIndex);
      }
      QStringList txItems;
      txItems << txHashStr << UiUtils::displayAmount(amount);
      auto txHashItem = new QTreeWidgetItem(txItems);
      if (!isOutput) {
         txHashItem->setData(0, Qt::UserRole, QString::fromStdString(txHash.toHexStr(true)));
      }
      txHashItem->setData(1, Qt::UserRole, (qulonglong)amount);
      item->addChild(txHashItem);
   }
}

// A function that sends a signal to the explorer widget to open the address
// details widget for a clicked address. Doesn't apply to OP_RETURN or non-std
// addresses.
void TransactionDetailsWidget::onAddressClicked(QTreeWidgetItem *item, int column)
{
   if (item->childCount() > 0) {
      emit addressClicked(item->text(colAddressId));
   }
   else {
      const auto txHashStr = item->data(colAddressId, Qt::UserRole).toString();
      if (!txHashStr.isEmpty()) {
         emit txHashClicked(txHashStr);
      }
   }
}

// Clear all the fields.
void TransactionDetailsWidget::clear()
{
   prevTxMap_.clear();
   curTx_ = Tx();
   inputItems_.clear();
   outputItems_.clear();

   ui_->tranID->clear();
   ui_->tranNumInputs->clear();
   ui_->tranNumOutputs->clear();
   ui_->tranInput->clear();
   ui_->tranOutput->clear();
   ui_->tranFees->clear();
   ui_->tranFeePerByte->clear();
   ui_->tranSize->clear();
   ui_->treeInput->clear();
   ui_->treeOutput->clear();
}
