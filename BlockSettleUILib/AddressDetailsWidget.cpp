#include "AddressDetailsWidget.h"
#include "ui_AddressDetailsWidget.h"

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
}

void AddressDetailsWidget::setAddrVal(const bs::Address& inAddrVal) {
   addrVal = inAddrVal;

   // setting the address field in address page to dummy text
   ui_->addressId->setText(tr("111111"));
   ui_->balance->setText(tr("0.123"));
   // The same can be done for all other fields such as balance, totalSent, totalReceived.
   // I think instead of creating a separate function for each the data should be sent all in one call
   // and unpacked here, but you can make this decision as you have a much better understanding of the armory piece.
}

void AddressDetailsWidget::setAddrVal(const QString inAddrVal) {
   ui_->addressId->setText(inAddrVal);
}

void AddressDetailsWidget::loadTransactions() {
   CustomTreeWidget *tree = ui_->treeAddressTransactions;
   tree->clear();
   double outputVal = -4;

   // here's the code to add data to the address tree, the tree.
   for (int i = 0; i < 10; i++) {
      QTreeWidgetItem *item = new QTreeWidgetItem(tree);
      item->setText(0, tr("6-Aug-2018 10:26:53")); // date
      item->setText(1, tr("2e76b709c10c585a28aa3fac76ea5736f63e678ff825618d78dbed36119dee21")); // tx id
      item->setText(2, QString(tr("%1")).arg(i)); // confirmations
      item->setText(3, tr("3")); // inputs #
      item->setText(4, tr("2")); // outputs #
      item->setText(5, QString::number(outputVal + i, 'G', 4)); // output
      //item->setText(5, tr("0.36742580")); // output
      item->setText(6, tr("-0.00850000")); // fees
      item->setText(7, tr("110.21")); // fee per byte
      item->setText(8, tr("0.521")); // size

      setConfirmationColor(item);
      // disabled as per Scott's request
      //setOutputColor(item);
      tree->addTopLevelItem(item);
   }
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
