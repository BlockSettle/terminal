#include "TransactionDetailsWidget.h"
#include "ui_TransactionDetailsWidget.h"
#include <QToolTip>

TransactionDetailsWidget::TransactionDetailsWidget(QWidget *parent) :
    QWidget(parent),
   ui_(new Ui::TransactionDetailsWidget)
{
   ui_->setupUi(this);
}

TransactionDetailsWidget::~TransactionDetailsWidget()
{
    delete ui_;
}

void TransactionDetailsWidget::setTxRefVal(const BinaryData& inTxRef) {
   txRefVal = inTxRef;

   // convert inTxRef to string and set it here
   ui_->tranID->setText(tr("transaction id is set here"));
}

void TransactionDetailsWidget::setTxVal(const QString inTx) {
   ui_->tranID->setText(inTx);
}

// This function populates the inputs tree with top level and child items. The exactly same code
// applies to ui_->treeOutput
void TransactionDetailsWidget::loadInputs() {
   QTreeWidget *tree = ui_->treeInput;
   tree->clear();
   // here's the code to add data to the Input tree.
   for (int i = 0; i < 5; i++) {
      // create a top level item using type, address, amount, wallet values
      QTreeWidgetItem *item = createItem(tree, tr("Input"), tr("1JSAGsDo56rEqgxf3R1EAiCgwGJCUB31Cr"), tr("-0.00850000"), tr("Settlement"));

      // add several child items to this top level item to crate a new branch in the tree
      item->addChild(createItem(item, tr("p2wsh"), tr("1JSAGsDo56rEqgxf3R1EAiCgwGJCUB31Cr"), tr("-0.00850000"), tr("Settlement")));
      item->addChild(createItem(item, tr("1JSAGsDo56rEqgxf3R1EAiCgwGJCUB31Cr"), tr("1JSAGsDo56rEqgxf3R1EAiCgwGJCUB31Cr"), tr("-0.00850000"), tr("Settlement")));
      item->addChild(createItem(item, tr("1JSAGsDo56rEqgxf3R1EAiCgwGJCUB31Cr"), tr("1JSAGsDo56rEqgxf3R1EAiCgwGJCUB31Cr"), tr("-0.00850000"), tr("Settlement")));

      // add the item to the tree
      tree->addTopLevelItem(item);
   }
}

QTreeWidgetItem * TransactionDetailsWidget::createItem(QTreeWidget *tree, QString type, QString address, QString amount, QString wallet) {
   QTreeWidgetItem *item = new QTreeWidgetItem(tree);
   item->setText(0, type); // type
   item->setText(1, address); // address
   item->setText(2, amount); // amount
   item->setText(3, wallet); // wallet
   return item;
}

QTreeWidgetItem * TransactionDetailsWidget::createItem(QTreeWidgetItem *parentItem, QString type, QString address, QString amount, QString wallet) {
   QTreeWidgetItem *item = new QTreeWidgetItem(parentItem);
   item->setFirstColumnSpanned(true);
   item->setText(0, type); // type
   item->setText(1, address); // address
   item->setText(2, amount); // amount
   item->setText(3, wallet); // wallet
   return item;
}
