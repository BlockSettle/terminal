#include "SelectAddressDialog.h"
#include "ui_SelectAddressDialog.h"

#include <QPushButton>

#include "Wallets/SyncWalletsManager.h"


SelectAddressDialog::SelectAddressDialog(const std::shared_ptr<bs::sync::WalletsManager> &walletsManager
   , const std::shared_ptr<bs::sync::Wallet>& wallet
   , QWidget* parent, AddressListModel::AddressType addrType)
 : QDialog(parent)
 , ui_(new Ui::SelectAddressDialog)
 , wallet_(wallet)
{
   ui_->setupUi(this);

   model_ = new AddressListModel(walletsManager, ui_->treeView, addrType);
   model_->setWallets({wallet_}, false, false);
   ui_->treeView->setModel(model_);

   ui_->treeView->header()->setSectionResizeMode(QHeaderView::ResizeToContents);

   connect(ui_->treeView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &SelectAddressDialog::onSelectionChanged);
   connect(ui_->treeView, &QTreeView::doubleClicked, this, &SelectAddressDialog::onDoubleClicked);

   connect(ui_->pushButtonCancel, &QPushButton::clicked, this, &SelectAddressDialog::reject);
   connect(ui_->pushButtonSelect, &QPushButton::clicked, this, &SelectAddressDialog::accept);

   onSelectionChanged();
}

SelectAddressDialog::~SelectAddressDialog() = default;

bs::Address SelectAddressDialog::getAddress(const QModelIndex& index) const
{
   return bs::Address(model_->data(index, AddressListModel::AddressRole).toString().toStdString());
}

void SelectAddressDialog::onSelectionChanged()
{
   auto selectedRows = ui_->treeView->selectionModel()->selectedRows();
   if (selectedRows.size() == 1) {
      selectedAddr_ = getAddress(selectedRows[0]);
   } else {
      selectedAddr_ = bs::Address();
   }

   ui_->pushButtonSelect->setEnabled(!selectedAddr_.isNull());
}

bs::Address SelectAddressDialog::getSelectedAddress() const
{
   return selectedAddr_;
}

void SelectAddressDialog::onDoubleClicked(const QModelIndex& index)
{
   selectedAddr_ = getAddress(index);
   if (!selectedAddr_.isNull()) {
      accept();
   }
}
