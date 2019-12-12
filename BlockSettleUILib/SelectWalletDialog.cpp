/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "ui_SelectWalletDialog.h"
#include "SelectWalletDialog.h"
#include <QPushButton>
#include "ApplicationSettings.h"
#include "WalletsViewModel.h"
#include "Wallets/SyncWalletsManager.h"


SelectWalletDialog::SelectWalletDialog(const std::shared_ptr<bs::sync::WalletsManager> &walletsManager
   , const std::string &selWalletId, QWidget* parent)
   : QDialog(parent)
   , ui_(new Ui::SelectWalletDialog)
   , walletsManager_(walletsManager)
{
   ui_->setupUi(this);

   ui_->buttonBox->button(QDialogButtonBox::Ok)->setText(tr("Select"));

   walletsModel_ = new WalletsViewModel(walletsManager, selWalletId, nullptr, ui_->treeViewWallets, true);
   walletsModel_->setBitcoinLeafSelectionMode();
   ui_->treeViewWallets->setModel(walletsModel_);
   ui_->treeViewWallets->setItemsExpandable(true);
   ui_->treeViewWallets->setRootIsDecorated(true);
   walletsModel_->LoadWallets();
   ui_->treeViewWallets->header()->setSectionResizeMode(QHeaderView::ResizeToContents);

   connect(ui_->treeViewWallets->selectionModel(), &QItemSelectionModel::selectionChanged
           , this, &SelectWalletDialog::onSelectionChanged);
   connect(ui_->treeViewWallets, &QTreeView::doubleClicked
           , this, &SelectWalletDialog::onDoubleClicked);

   connect(ui_->buttonBox, &QDialogButtonBox::accepted, this, &SelectWalletDialog::accept);
   connect(ui_->buttonBox, &QDialogButtonBox::rejected, this, &SelectWalletDialog::reject);

   onSelectionChanged();
   ui_->treeViewWallets->expandAll();
}

SelectWalletDialog::~SelectWalletDialog() = default;

void SelectWalletDialog::onSelectionChanged()
{
   auto selectedRows = ui_->treeViewWallets->selectionModel()->selectedRows();
   if (selectedRows.size() == 1) {
      selectedWallet_ = walletsModel_->getWallet(selectedRows[0]);
   }
   else {
      selectedWallet_ = nullptr;
   }
   walletsModel_->setSelectedWallet(selectedWallet_);

   auto okButton = ui_->buttonBox->button(QDialogButtonBox::Ok);
   okButton->setEnabled(selectedWallet_ != nullptr);
}

std::shared_ptr<bs::sync::Wallet> SelectWalletDialog::getSelectedWallet() const
{
   return selectedWallet_;
}

void SelectWalletDialog::onDoubleClicked(const QModelIndex& index)
{
   if (!selectedWallet_) {
      return;
   }

   selectedWallet_ = walletsModel_->getWallet(index);
   QDialog::accept();
}
