#include "TransactionDetailsWidget.h"
#include "ui_TransactionDetailsWidget.h"
#include "BTCNumericTypes.h"
#include "BlockObj.h"
#include "UiUtils.h"

#include <memory>
#include <QToolTip>

Q_DECLARE_METATYPE(Tx);

TransactionDetailsWidget::TransactionDetailsWidget(QWidget *parent) :
    QWidget(parent),
   ui_(new Ui::TransactionDetailsWidget)
{
   ui_->setupUi(this);

   qRegisterMetaType<Tx>();

   // setting up a tooltip that pops up immediately when mouse hovers over it
   QIcon btcIcon(QLatin1String(":/resources/notification_info.png"));
   ui_->labelTxPopup->setPixmap(btcIcon.pixmap(13, 13));
   ui_->labelTxPopup->setMouseTracking(true);
   ui_->labelTxPopup->toolTip_ = tr("The Transaction ID (TXID) is in big " \
                                    "endian notation. It will differ from the " \
                                    "user input if the input is little endian.");

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

TransactionDetailsWidget::~TransactionDetailsWidget()
{
    delete ui_;
}

// Initialize the widget and related widgets (block, address, Tx)
void TransactionDetailsWidget::init(const std::shared_ptr<ArmoryConnection> &armory,
                                    const std::shared_ptr<spdlog::logger> &inLogger)
{
   armory_ = armory;
   logger_ = inLogger;
}

// This function tries to use getTxByHash() to retrieve info about transaction. 
// Code was commented out because I could not get it to work and didn't want to spend too
// much time trying. At the bottom of the function are 2 calls, one that populates 
// transaction id in the page, and the other populates Inputs tree.
void TransactionDetailsWidget::populateTransactionWidget(BinaryData inHex,
                                                         const bool& firstPass) {
   // get the transaction data from armory
   const auto &cbTX = [this, &inHex, firstPass](Tx tx) {
      if (!tx.isInitialized()) {
         logger_->error("[TransactionDetailsWidget::populateTransactionWidget] TX not " \
                        "initialized for hash {}.",
                        inHex.toHexStr());
         // If failure, try swapping the endian. We want big endian data.
         if(firstPass == true) {
            BinaryData inHexBE = inHex.swapEndian();
            populateTransactionWidget(inHexBE, false);
         }
         return;
      }

      // UI changes in a non-main thread will trigger a crash. Invoke a new
      // thread to handle the received data. (UI changes happen eventually.)
      QMetaObject::invokeMethod(this, "processTxData", Qt::QueuedConnection
         , Q_ARG(Tx, tx));
   };
   armory_->getTxByHash(inHex.swapEndian(), cbTX);
}

// Used in callback - FIX DESCRIPTION
void TransactionDetailsWidget::processTxData(Tx tx) {
   // Save Tx and the prev Tx entries (get input amounts & such)
   curTx = tx;

   const auto &cbProcessTX = [this](std::vector<Tx> prevTxs) {
      for (const auto &prevTx : prevTxs) {
         const auto &txHash = prevTx.getThisHash();
         prevTxHashSet.erase(txHash); // If empty, use this to make a relevant call.
         prevTxMap_[txHash] = prevTx;
      }

      // We're finally ready to display all the transactions.
      setTxGUIValues();
   };

   // While here, we need to get the prev Tx with the UTXO being spent.
   // This is done so that we can calculate fees later.
   for (size_t i = 0; i < tx.getNumTxIn(); i++) {
      TxIn in = tx.getTxInCopy(i);
      OutPoint op = in.getOutPoint();
      const auto &itTX = prevTxMap_.find(op.getTxHash());
      if(itTX == prevTxMap_.end()) {
         prevTxHashSet.insert(op.getTxHash());
      }
   }
   if(!prevTxHashSet.empty()) {
      armory_->getTXsByHash(prevTxHashSet, cbProcessTX);
   }
}

// WARNING: Don't use ClientClasses::BlockHeader. It has parsing capabilities
// but it's meant to be an internal Armory class, touching things like the DB.
// Just parse the raw data header here.
void TransactionDetailsWidget::getHeaderData(const BinaryData& inHeader)
{
   if(inHeader.getSize() != 80)
   {
      logger_->error("[TransactionDetailWidgets::getHeaderData] Header is " \
                     "not the correct size - size = {}", inHeader.getSize());
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

void TransactionDetailsWidget::setTxGUIValues()
{
   // Get Tx header data.
   BinaryData txHdr(curTx.getPtr(), 80);
   getHeaderData(txHdr);

   // Get fees & fee/byte by looping through the prev Tx set and calculating.
   uint64_t totIn = 0;
   for(size_t r = 0; r < curTx.getNumTxIn(); ++r) {
      TxIn in = curTx.getTxInCopy(r);
      OutPoint op = in.getOutPoint();
      const auto &prevTx = prevTxMap_[op.getTxHash()];
      if (prevTx.isInitialized()) {
         TxOut prevOut = prevTx.getTxOutCopy(op.getTxOutIndex());
         totIn += prevOut.getValue();
      }
   }
   uint64_t fees = totIn - curTx.getSumOfOutputs();
   double feePerByte = (double)fees / (double)curTx.getSize();

   // Populate the GUI fields.
   ui_->tranID->setText(QString::fromStdString(curTx.getThisHash().toHexStr()));
//   ui_->tranDate->setText(UiUtils::displayDateTime(QDateTime::fromTime_t(curTxTimestamp)));
   ui_->tranDate->setText(QString::fromStdString("Timestamp here"));        // FIX ME!!!
   ui_->tranHeight->setText(QString::fromStdString("Height here"));         // FIX ME!!!
   ui_->tranConfirmations->setText(QString::fromStdString("# confs here")); // FIX ME!!!
   ui_->tranNumInputs->setText(QString::number(curTx.getNumTxIn()));
   ui_->tranNumOutputs->setText(QString::number(curTx.getNumTxOut()));
   ui_->tranOutput->setText(QString::number(curTx.getSumOfOutputs() / BTCNumericTypes::BalanceDivider,
                                            'f',
                                            BTCNumericTypes::default_precision));
   ui_->tranFees->setText(QString::number(fees / BTCNumericTypes::BalanceDivider,
                                          'f',
                                          BTCNumericTypes::default_precision));
   ui_->tranFeePerByte->setText(QString::number(nearbyint(feePerByte)));
   ui_->tranSize->setText(QString::number(curTx.getSize()));

   loadInputs();
}

// This function populates the inputs tree with top level and child items. The exactly same code
// applies to ui_->treeOutput
void TransactionDetailsWidget::loadInputs() {
   // for testing purposes i populate both trees with test data
   loadTreeIn(ui_->treeInput);
   loadTreeOut(ui_->treeOutput);
}

void TransactionDetailsWidget::loadTreeIn(CustomTreeWidget *tree) {
   tree->clear();

   // here's the code to add data to the Input tree.
   for (size_t i = 0; i < curTx.getNumTxIn(); i++) {
      TxOut prevOut;
      TxIn in = curTx.getTxInCopy(i);
      OutPoint op = in.getOutPoint();
      const auto &prevTx = prevTxMap_[op.getTxHash()];
      if (prevTx.isInitialized()) {
         prevOut = prevTx.getTxOutCopy(op.getTxOutIndex());
      }
      const auto outAddr = bs::Address::fromTxOut(prevOut);
      double amtBTC = prevOut.getValue() / BTCNumericTypes::BalanceDivider;

      // create a top level item using type, address, amount, wallet values
      QTreeWidgetItem *item = createItem(tree,
                                         tr("Input"),
                                         outAddr.display(),
                                         QString::number(amtBTC,
                                                         'f',
                                                         BTCNumericTypes::default_precision),
                                         tr("Settlement"));

      // add several child items to this top level item to crate a new branch in the tree
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

void TransactionDetailsWidget::loadTreeOut(CustomTreeWidget *tree) {
   tree->clear();

   // here's the code to add data to the Input tree.
   for (size_t i = 0; i < curTx.getNumTxOut(); i++) {
      const auto outAddr = bs::Address::fromTxOut(curTx.getTxOutCopy(i));
      double amtBTC = curTx.getTxOutCopy(i).getValue() / BTCNumericTypes::BalanceDivider;

      // create a top level item using type, address, amount, wallet values
      QTreeWidgetItem *item = createItem(tree,
                                         tr("Output"),
                                         outAddr.display(),
                                         QString::number(amtBTC,
                                                         'f',
                                                         BTCNumericTypes::default_precision),
                                         tr("Settlement"));

      // add several child items to this top level item to crate a new branch in the tree
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

QTreeWidgetItem * TransactionDetailsWidget::createItem(QTreeWidget *tree,
                                                       QString type,
                                                       QString address,
                                                       QString amount,
                                                       QString wallet)
{
   QTreeWidgetItem *item = new QTreeWidgetItem(tree);
   item->setText(0, type); // type
   item->setText(1, address); // address
   item->setText(2, amount); // amount
   item->setText(3, wallet); // wallet
   return item;
}

QTreeWidgetItem * TransactionDetailsWidget::createItem(QTreeWidgetItem *parentItem,
                                                       QString type,
                                                       QString address,
                                                       QString amount,
                                                       QString wallet) {
   QTreeWidgetItem *item = new QTreeWidgetItem(parentItem);
   item->setFirstColumnSpanned(true);
   item->setText(0, type); // type
   item->setText(1, address); // address
   item->setText(2, amount); // amount
   item->setText(3, wallet); // wallet
   return item;
}

void TransactionDetailsWidget::onAddressClicked(QTreeWidgetItem *item, int column) {
   // user has clicked the address column of the item so
   // send a signal to ExplorerWidget to open AddressDetailsWidget
   if (column == colAddressId) {
      emit(addressClicked(item->text(colAddressId)));
   }
}
