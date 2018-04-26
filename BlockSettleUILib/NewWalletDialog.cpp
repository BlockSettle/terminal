#include "NewWalletDialog.h"
#include "ui_NewWalletDialog.h"


NewWalletDialog::NewWalletDialog(bool noWalletsFound, QWidget *parent)
   : QDialog(parent)
   , ui_(new Ui::NewWalletDialog)
{
   ui_->setupUi(this);

   if (noWalletsFound) {
      ui_->labelPurpose->setText(tr("THE TERMINAL CAN'T FIND ANY EXISTING WALLETS"));
   }

   connect(ui_->pushButtonCreate, &QPushButton::clicked, [this] {
      isCreate_ = true;
      accept();
   });
   connect(ui_->pushButtonImport, &QPushButton::clicked, [this] {
      isImport_ = true;
      accept();
   });
}
