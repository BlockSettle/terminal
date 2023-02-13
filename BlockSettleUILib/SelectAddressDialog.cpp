/*

***********************************************************************************
* Copyright (C) 2018 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "SelectAddressDialog.h"
#include "ui_SelectAddressDialog.h"

#include <QPushButton>

#include "Wallets/SyncWalletsManager.h"


SelectAddressDialog::SelectAddressDialog(QWidget* parent
   , AddressListModel::AddressType addrType)
   : QDialog(parent)
   , ui_(new Ui::SelectAddressDialog)
   , addrType_(addrType)
{
   ui_->setupUi(this);

   model_ = std::make_unique<AddressListModel>(ui_->treeView, addrType);
   connect(model_.get(), &AddressListModel::needExtAddresses, this, &SelectAddressDialog::needExtAddresses);
   connect(model_.get(), &AddressListModel::needIntAddresses, this, &SelectAddressDialog::needIntAddresses);
   connect(model_.get(), &AddressListModel::needUsedAddresses, this, &SelectAddressDialog::needUsedAddresses);
   connect(model_.get(), &AddressListModel::needAddrComments, this, &SelectAddressDialog::needAddrComments);

   ui_->treeView->setModel(model_.get());

   ui_->treeView->header()->setSectionResizeMode(QHeaderView::ResizeToContents);

   connect(ui_->treeView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &SelectAddressDialog::onSelectionChanged);
   connect(ui_->treeView, &QTreeView::doubleClicked, this, &SelectAddressDialog::onDoubleClicked);

   connect(ui_->pushButtonCancel, &QPushButton::clicked, this, &SelectAddressDialog::reject);
   connect(ui_->pushButtonSelect, &QPushButton::clicked, this, &SelectAddressDialog::accept);
}

SelectAddressDialog::~SelectAddressDialog() = default;

void SelectAddressDialog::setWallets(const AddressListModel::Wallets& wallets)
{
   model_->setWallets(wallets, false, false);
   onSelectionChanged();
}

void SelectAddressDialog::onAddresses(const std::string& walletId
   , const std::vector<bs::sync::Address>& addrs)
{
   model_->onAddresses(walletId, addrs);
}

void SelectAddressDialog::onAddressComments(const std::string& walletId
   , const std::map<bs::Address, std::string>& addrComments)
{
   model_->onAddressComments(walletId, addrComments);
}

void SelectAddressDialog::onAddressBalances(const std::string& walletId
   , const std::vector<bs::sync::WalletBalanceData::AddressBalance>& addrBal)
{
   model_->onAddressBalances(walletId, addrBal);
}

#if 0
void SelectAddressDialog::init()
{
   ui_->setupUi(this);

   model_ = std::make_unique<AddressListModel>(walletsMgr_, ui_->treeView, addrType_);
   std::vector<bs::sync::WalletInfo> walletsInfo;
   for (const auto &wallet : wallets_) {
      const auto leaf = std::dynamic_pointer_cast<bs::sync::hd::Leaf>(wallet);
      walletsInfo.push_back(bs::sync::WalletInfo::fromLeaf(leaf));
   }
   model_->setWallets(walletsInfo, false, false);
   ui_->treeView->setModel(model_.get());

   ui_->treeView->header()->setSectionResizeMode(QHeaderView::ResizeToContents);

   connect(ui_->treeView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &SelectAddressDialog::onSelectionChanged);
   connect(ui_->treeView, &QTreeView::doubleClicked, this, &SelectAddressDialog::onDoubleClicked);

   connect(ui_->pushButtonCancel, &QPushButton::clicked, this, &SelectAddressDialog::reject);
   connect(ui_->pushButtonSelect, &QPushButton::clicked, this, &SelectAddressDialog::accept);

   onSelectionChanged();
}
#endif   //0

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

   ui_->pushButtonSelect->setEnabled(!selectedAddr_.empty());
}

bs::Address SelectAddressDialog::getSelectedAddress() const
{
   return selectedAddr_;
}

void SelectAddressDialog::onDoubleClicked(const QModelIndex& index)
{
   selectedAddr_ = getAddress(index);
   if (selectedAddr_.isValid()) {
      accept();
   }
}
