#include "AddressDetailsWidget.h"
#include "ui_AddressDetailsWidget.h"
#include <QDebug>

AddressDetailsWidget::AddressDetailsWidget(QWidget *parent) :
    QWidget(parent),
   ui_(new Ui::AddressDetailsWidget)
{
   ui_->setupUi(this);

   connect(ui_->treeAddressTransactions->selectionModel(), &QItemSelectionModel::selectionChanged,
      this, &AddressDetailsWidget::onSelectionChanged);
   connect(ui_->treeAddressTransactions, &QTreeWidget::itemClicked,
      this, &AddressDetailsWidget::onItemClicked);
}

AddressDetailsWidget::~AddressDetailsWidget() {
    delete ui_;
}

void AddressDetailsWidget::setAddrVal(const bs::Address& inAddrVal) {
   addrVal = inAddrVal;

   // setting the address field in address page to dummy text
   ui_->addressId->setText(tr("11111"));
   ui_->balance->setText(tr("zero balance"));
   // The same can be done for all other fields such as balance, totalSent, totalReceived.
   // I think instead of creating a separate function for each the data should be sent all in one call
   // and unpacked here, but you can make this decision as you have a much better understanding of the armory piece.
}

void AddressDetailsWidget::loadTransactions() {
   QTreeWidget *tree = ui_->treeAddressTransactions;
   tree->clear();
   // here's the code to add data to the address tree, the tree.
   for (int i = 0; i < 10; i++) {
      QTreeWidgetItem *item = new QTreeWidgetItem(tree);
      item->setText(0, tr("6-Aug-2018 10:26:53")); // date
      item->setText(1, tr("2e76b709c10c585a28aa3fac76ea5736f63e678ff825618d78dbed36119dee21")); // tx id
      item->setText(2, QString(tr("%1")).arg(i)); // confirmations
      item->setText(3, tr("3")); // inputs #
      item->setText(4, tr("2")); // outputs #
      item->setText(5, tr("0.36742580")); // output
      item->setText(6, tr("-0.00850000")); // fees
      item->setText(7, tr("110.21")); // fee per byte
      item->setText(8, tr("0.521")); // size

      setConfirmationColor(item);
      tree->addTopLevelItem(item);
   }
}

// This function sets the confirmation column to the correct color based
// on the number of confirmations that are set inside the column.
void AddressDetailsWidget::setConfirmationColor(QTreeWidgetItem *item) {
   int conf = item->text(2).toInt();
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
   item->setForeground(2, brush);
}

void AddressDetailsWidget::onSelectionChanged(const QItemSelection &selected, const QItemSelection &deselected) {
   qDebug() << "AddressDetailsWidget::onSelectionChanged column";
}

void AddressDetailsWidget::onItemClicked(QTreeWidgetItem *item, int column) {
   qDebug() << "AddressDetailsWidget::itemClicked column" << column;
   // user has clicked the transaction of the item
   if (column == 1) {

      emit(transactionClicked(item->text(1)));
   }
}