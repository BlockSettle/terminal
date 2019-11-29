/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "SelectAddressDialog.h"
#include "ui_SelectAddressDialog.h"

#include <QPushButton>

#include "Wallets/SyncWalletsManager.h"


SelectAddressDialog::SelectAddressDialog(const std::shared_ptr<bs::sync::WalletsManager> &walletsManager
   , const std::shared_ptr<bs::sync::Wallet>& wallet
   , QWidget* parent, AddressListModel::AddressType addrType)
   : QDialog(parent)
   , ui_(new Ui::SelectAddressDialog)
   , wallets_({ wallet })
   , walletsMgr_(walletsManager)
   , addrType_(addrType)
{
   init();
}

SelectAddressDialog::SelectAddressDialog(const std::shared_ptr<bs::sync::hd::Group> &group
   , QWidget* parent, AddressListModel::AddressType addrType)
   : QDialog(parent)
   , ui_(new Ui::SelectAddressDialog)
   , wallets_(group->getAllLeaves())
   , addrType_(addrType)
{
   init();
}

SelectAddressDialog::~SelectAddressDialog() = default;

void SelectAddressDialog::init()
{
   ui_->setupUi(this);

   model_ = std::make_unique<AddressListModel>(walletsMgr_, ui_->treeView, addrType_);
   model_->setWallets(wallets_, false, false);
   ui_->treeView->setModel(model_.get());

   ui_->treeView->header()->setSectionResizeMode(QHeaderView::ResizeToContents);

   connect(ui_->treeView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &SelectAddressDialog::onSelectionChanged);
   connect(ui_->treeView, &QTreeView::doubleClicked, this, &SelectAddressDialog::onDoubleClicked);

   connect(ui_->pushButtonCancel, &QPushButton::clicked, this, &SelectAddressDialog::reject);
   connect(ui_->pushButtonSelect, &QPushButton::clicked, this, &SelectAddressDialog::accept);

   onSelectionChanged();
}

bs::Address SelectAddressDialog::getAddress(const QModelIndex& index) const
{
   return bs::Address::fromAddressString(model_->data(index, AddressListModel::AddressRole).toString().toStdString());
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
