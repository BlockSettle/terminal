#include "TransactionDetailsWidget.h"
#include "ui_TransactionDetailsWidget.h"
#include "BTCNumericTypes.h"
#include "BlockObj.h"
#include "UiUtils.h"

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
   , const std::shared_ptr<QTimer> &inTimer)
{
   armory_ = armory;
   logger_ = inLogger;
   expTimer_ = inTimer;

   connect(armory_.get(), &ArmoryConnection::newBlock, this
      , &TransactionDetailsWidget::onNewBlock, Qt::QueuedConnection);
}

// This function uses getTxByHash() to retrieve info about transaction. The
// incoming TXID must be in RPC order, not internal order.
void TransactionDetailsWidget::populateTransactionWidget(BinaryTXID rpcTXID,
                                                         const bool& firstPass)
{
   if (!armory_) {
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
   const auto &cbTX = [this, txidStr](Tx tx) {
      if (tx.isInitialized()) {
         processTxData(tx);
      }
      else if (logger_) {
         logger_->error("[{}] TXID {} is not initialized.", __func__, txidStr);
      }
   };

   // The TXID passed to Armory *must* be in internal order!
   if (!armory_->getTxByHash(rpcTXID.getInternalTXID(), cbTX)) {
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
   const auto &cbProcessTX = [this](std::vector<Tx> prevTxs) {
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
      armory_->getTXsByHash(prevTxHashSet, cbProcessTX);
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

   // It's now safe to stop the query expiration timer. Do it right away.
   expTimer_->stop();

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

// Input widget population.
void TransactionDetailsWidget::loadTreeIn(CustomTreeWidget *tree)
{
   tree->clear();

   // here's the code to add data to the Input tree.
   for (size_t i = 0; i < curTx_.getNumTxIn(); i++) {
      TxOut prevOut;
      TxIn in = curTx_.getTxInCopy(i);
      OutPoint op = in.getOutPoint();
      BinaryTXID intPrevTXID(op.getTxHash(), false);
      const auto &prevTx = prevTxMap_[intPrevTXID];
      if (prevTx.isInitialized()) {
         prevOut = prevTx.getTxOutCopy(op.getTxOutIndex());
      }
      auto txType = prevOut.getScriptType();
      const auto outAddr = bs::Address::fromTxOut(prevOut);
      double amtBTC = UiUtils::amountToBtc(prevOut.getValue());
      QString typeStr;
      QString addrStr;

      // For now, don't display any data if the TxOut is non-std. Displaying a
      // hex version of the script is one thing that could be done. This needs
      // to be discussed before implementing. Non-std could mean many things.
      if (txType == TXOUT_SCRIPT_NONSTANDARD) {
         typeStr = QString::fromStdString("Non-Std");
      }
      else {
         typeStr = QString::fromStdString("Input");
         addrStr = outAddr.display();
      }

      // create a top level item using type, address, amount, wallet values
      QTreeWidgetItem *item = createItem(tree, typeStr, addrStr
         , UiUtils::displayAmount(amtBTC), QString());

      // Example: Add several child items to this top level item to crate a new
      // branch in the tree. Could be useful for things like expanding a non-std
      // input, or expanding any input, really.
/*      item->addChild(createItem(item,
                                tr("1JSAGsDo56rEqgxf3R1EAiCgwGJCUB31Cr"),
                                tr("1JSAGsDo56rEqgxf3R1EAiCgwGJCUB31Cr"),
                                tr("-0.00850000"),
                                tr("Settlement")));
      item->setExpanded(true);*/

      // add the item to the tree
      tree->addTopLevelItem(item);
   }
   tree->resizeColumns();
}

// Output widget population.
void TransactionDetailsWidget::loadTreeOut(CustomTreeWidget *tree)
{
   tree->clear();

   // here's the code to add data to the Input tree.
   for (size_t i = 0; i < curTx_.getNumTxOut(); i++) {
      auto txType = curTx_.getTxOutCopy(i).getScriptType();
      const auto outAddr = bs::Address::fromTxOut(curTx_.getTxOutCopy(i));
      double amtBTC = UiUtils::amountToBtc(curTx_.getTxOutCopy(i).getValue());
      QString typeStr;
      QString addrStr;

      // For now, don't display any data if the TxOut is OP_RETURN or non-std.
      // Displaying a hex version of the script is one thing that could be done.
      // This needs to be discussed before implementing. OP_RETURN isn't too bad
      // (80 bytes max) but non-std could mean just about anything.
      if (txType == TXOUT_SCRIPT_OPRETURN) {
         typeStr = QString::fromStdString("OP_RETURN");
      }
      else if (txType == TXOUT_SCRIPT_NONSTANDARD) {
         typeStr = QString::fromStdString("Non-Std");
      }
      else {
         typeStr = QString::fromStdString("Output");
         addrStr = outAddr.display();
      }

      // create a top level item using type, address, amount, wallet values
      QTreeWidgetItem *item = createItem(tree, typeStr, addrStr
         , UiUtils::displayAmount(amtBTC), QString());

      // add the item to the tree
      tree->addTopLevelItem(item);
   }
   tree->resizeColumns();
}

QTreeWidgetItem * TransactionDetailsWidget::createItem(QTreeWidget *tree,
                                                       QString type,
                                                       QString address,
                                                       QString amount,
                                                       QString wallet)
{
   QTreeWidgetItem *item = new QTreeWidgetItem(tree);
   item->setText(colType, type); // type
   item->setText(colAddressId, address); // address
   item->setText(colAmount, amount); // amount
   item->setText(colWallet, wallet); // wallet
   return item;
}

QTreeWidgetItem * TransactionDetailsWidget::createItem(QTreeWidgetItem *parentItem,
                                                       QString type,
                                                       QString address,
                                                       QString amount,
                                                       QString wallet)
{
   QTreeWidgetItem *item = new QTreeWidgetItem(parentItem);
   item->setFirstColumnSpanned(true);
   item->setText(colType, type); // type
   item->setText(colAddressId, address); // address
   item->setText(colAmount, amount); // amount
   item->setText(colWallet, wallet); // wallet
   return item;
}

// A function that sends a signal to the explorer widget to open the address
// details widget for a clicked address. Doesn't apply to OP_RETURN or non-std
// addresses.
void TransactionDetailsWidget::onAddressClicked(QTreeWidgetItem *item, int column)
{
   if (column == colAddressId) {
      auto typeText = item->text(colType);
      if (typeText == QString::fromStdString("Input")
         || typeText == QString::fromStdString("Output")) {
         emit(addressClicked(item->text(colAddressId)));
      }
   }
}

// Clear all the fields.
void TransactionDetailsWidget::clear()
{
   prevTxMap_.clear();
   curTx_ = Tx();

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
