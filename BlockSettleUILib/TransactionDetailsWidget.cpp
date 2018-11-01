#include "TransactionDetailsWidget.h"
#include "ui_TransactionDetailsWidget.h"
#include "BTCNumericTypes.h"
#include "BlockObj.h"

#include <memory>
//#include <cmath>
#include <QToolTip>

TransactionDetailsWidget::TransactionDetailsWidget(QWidget *parent) :
    QWidget(parent),
   ui_(new Ui::TransactionDetailsWidget)
{
   ui_->setupUi(this);

   // setting up a tooltip that pops up immediately when mouse hovers over it
   QIcon btcIcon(QLatin1String(":/resources/notification_info.png"));
   ui_->labelTxPopup->setPixmap(btcIcon.pixmap(13, 13));
   ui_->labelTxPopup->setMouseTracking(true);
   ui_->labelTxPopup->toolTip_ = tr("The Transaction ID (TXID) is in big endian notation. It will differ from the user input if the input is little endian.");

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

void TransactionDetailsWidget::setTxVal(const QString inTx)
{
   ui_->tranID->setText(inTx);
}

void TransactionDetailsWidget::setTx(const Tx& inTx)
{
   curTx = inTx;
}

void TransactionDetailsWidget::setTxs(const std::vector<Tx> inTxs,
                                      const std::map<BinaryData, std::set<uint32_t>> inIndices)
{
   curTxs = inTxs;
   curIndices = inIndices;
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

   curTxVersion = READ_UINT32_LE(inHeader.getPtr());
   curTxPrevHash = BinaryData(inHeader.getPtr() + 4, 32);
   curTxMerkleRoot = BinaryData(inHeader.getPtr() + 36, 32);
   curTxTimestamp = READ_UINT32_LE(inHeader.getPtr() + 68);
   curTxDifficulty = BinaryData(inHeader.getPtr() + 72, 4);
   curTxNonce = READ_UINT32_LE(inHeader.getPtr() + 76);
}

void TransactionDetailsWidget::setTxGUIValues()
{
   // In order to calc the fees, we have to resolve each TxIn.

   uint64_t inSat = 0;
   std::vector<Tx> receivedTxs;

   // Process the received transactions
   for(const auto& resolvedTx : curTxs) {
      // Sanity check.
      if(!resolvedTx.isInitialized()) {
         logger_->error("[TransactionDetailsWidget::setTxGUIValues] TX " \
                        "not initialized - Fees");
         return;
      }

      // Sanity check.
      const auto &itTxOut = curIndices.find(resolvedTx.getThisHash());
      if (itTxOut == curIndices.end()) {
         logger_->error("[TransactionDetailsWidget::setTxGUIValues] TX " \
                        "not initialized - Fees 2"); // FIX ME!!!
         return;
      }

      // Get the appropriate TxOut Satoshis
      for(const auto &txOutIdx : itTxOut->second) {
         inSat += resolvedTx.getTxOutCopy(txOutIdx).getValue();
      }
   }

   // TxOut calculations are much simpler.
   uint64_t outSat = 0;
   for(size_t j = 0; j < curTx.getNumTxOut(); ++j) {
      outSat += curTx.getTxOutCopy(j).getValue();
   }


   // Calc the fees
   uint64_t txFeeSats = inSat - outSat;
   double txFee = double(txFeeSats) / double(BTCNumericTypes::BalanceDivider);
   double outAmt = double(outSat) / double(BTCNumericTypes::BalanceDivider);
   uint32_t txSize = curTx.getSize();
   uint32_t satPerByte = int(round(txFeeSats / txSize));

   // Get the appropriate block header and extract data as needed.
   // TO DO - DISABLED FOR NOW DUE TO A CRASH. getHeaderByHeight(), an almost
   // completely similar call, does work. This needs to be sorted out.
/*   const auto &cbHdr = [this](BinaryData blockHdr) {
      getHeaderData(blockHdr);
   };
   if(armory_->getRawHeaderForTxHash(curTx.getThisHash(), cbHdr) == false)
   {
      logger_->error("[TransactionDetailsWidget::setTxGUIValues] Unable to " \
                     "get block header for Tx hash {}",
                     curTx.getThisHash().toHexStr());
      return;
   }*/

   // Populate the GUI fields.
   ui_->tranID->setText(QString::fromStdString(curTx.getThisHash().toHexStr()));
//   ui_->tranDate->setText(QString::fromStdString(to_string(curTxTimestamp)));
   ui_->tranDate->setText(QString::fromStdString("Timestamp here"));        // FIX ME!!!
   ui_->tranHeight->setText(QString::fromStdString("Height here"));         // FIX ME!!!
   ui_->tranConfirmations->setText(QString::fromStdString("# confs here")); // FIX ME!!!
   ui_->tranNumInputs->setText(QString::fromStdString(to_string(curTx.getNumTxIn())));
   ui_->tranNumOutputs->setText(QString::fromStdString(to_string(curTx.getNumTxOut())));
   ui_->tranOutput->setText(QLocale().toString(outAmt,
                                               'f',
                                               BTCNumericTypes::default_precision));
   ui_->tranFees->setText(QLocale().toString(txFee,
                                             'f',
                                             BTCNumericTypes::default_precision));
   ui_->tranFeePerByte->setText(QString::fromStdString(to_string(satPerByte)));
   ui_->tranSize->setText(QString::fromStdString(to_string(txSize)));
}

// Take a Tx, get the set of Txs attached to the TxIns, and set the appropriate
// values.
void TransactionDetailsWidget::getTxsForTxIns()
{
   if(!curTx.isInitialized()) {
      logger_->error("[TransactionDetailWidgets::getTxsForTxIns] TX not " \
                     "initialized - Entry");
         return;
   }

   std::set<BinaryData> txHashSet;
   std::map<BinaryData, std::set<uint32_t>> txOutIndices;
   for(size_t i = 0; i < curTx.getNumTxIn(); ++i) {
      OutPoint op = curTx.getTxInCopy(i).getOutPoint();
      txHashSet.insert(op.getTxHash());
      txOutIndices[op.getTxHash()].insert(op.getTxOutIndex());
   }

   // Get the Txs associated with the TxIns.
   const auto &cbTXs = [this, txOutIndices](std::vector<Tx> txs) {
      this->setTxs(txs, txOutIndices);
   };
   armory_->getTXsByHash(txHashSet, cbTXs);

   setTxGUIValues();
}

// This function populates the inputs tree with top level and child items. The exactly same code
// applies to ui_->treeOutput
void TransactionDetailsWidget::loadInputs() {
   // for testing purposes i populate both trees with test data
   loadTree(ui_->treeInput);
   loadTree(ui_->treeOutput);
}

void TransactionDetailsWidget::loadTree(CustomTreeWidget *tree) {
   tree->clear();

   // here's the code to add data to the Input tree.
   for (int i = 0; i < 5; i++) {
      // create a top level item using type, address, amount, wallet values
      QTreeWidgetItem *item = createItem(tree, tr("Input"), tr("2JSAGsDo56rEqgxf3R1EAiCgwGJCUB31Cr"), tr("0.00850000"), tr("Settlement"));

      // add several child items to this top level item to crate a new branch in the tree
      item->addChild(createItem(item, tr("1JSAGsDo56rEqgxf3R1EAiCgwGJCUB31Cr"), tr("1JSAGsDo56rEqgxf3R1EAiCgwGJCUB31Cr"), tr("-0.00850000"), tr("Settlement")));

      item->setExpanded(true);
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
