/*

***********************************************************************************
* Copyright (C) 2018 - 2020, BlockSettle AB
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


SelectWalletDialog::SelectWalletDialog(const std::string& selWalletId, QWidget* parent)
   : QDialog(parent)
   , ui_(new Ui::SelectWalletDialog)
{
   ui_->setupUi(this);

   ui_->buttonBox->button(QDialogButtonBox::Ok)->setText(tr("Select"));

   walletsModel_ = new WalletsViewModel(selWalletId, ui_->treeViewWallets, true);
   walletsModel_->setBitcoinLeafSelectionMode();
   ui_->treeViewWallets->setModel(walletsModel_);
   ui_->treeViewWallets->setItemsExpandable(true);
   ui_->treeViewWallets->setRootIsDecorated(true);

   connect(ui_->treeViewWallets->selectionModel(), &QItemSelectionModel::selectionChanged
      , this, &SelectWalletDialog::onSelectionChanged);
   connect(ui_->treeViewWallets, &QTreeView::doubleClicked
      , this, &SelectWalletDialog::onDoubleClicked);

   connect(ui_->buttonBox, &QDialogButtonBox::accepted, this, &SelectWalletDialog::accept);
   connect(ui_->buttonBox, &QDialogButtonBox::rejected, this, &SelectWalletDialog::reject);
}

SelectWalletDialog::~SelectWalletDialog() = default;

void SelectWalletDialog::onSelectionChanged()
{
   auto selectedRows = ui_->treeViewWallets->selectionModel()->selectedRows();
   if (selectedRows.size() == 1) {
      selectedWallet_ = *walletsModel_->getWallet(selectedRows[0]).ids.cbegin();
   }
   else {
      selectedWallet_.clear();
   }
   walletsModel_->setSelectedWallet(selectedWallet_);

   auto okButton = ui_->buttonBox->button(QDialogButtonBox::Ok);
   okButton->setEnabled(!selectedWallet_.empty());
}

std::string SelectWalletDialog::getSelectedWallet() const
{
   return selectedWallet_;
}

void SelectWalletDialog::onHDWallet(const bs::sync::WalletInfo& wi)
{
   walletsModel_->onHDWallet(wi);
}

void SelectWalletDialog::onHDWalletDetails(const bs::sync::HDWalletData& hdw)
{
   walletsModel_->onHDWalletDetails(hdw);
   ui_->treeViewWallets->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
   onSelectionChanged();
   ui_->treeViewWallets->expandAll();
}

void SelectWalletDialog::onWalletBalances(const bs::sync::WalletBalanceData& wbd)
{
   walletsModel_->onWalletBalances(wbd);
}

void SelectWalletDialog::onDoubleClicked(const QModelIndex& index)
{
   if (selectedWallet_.empty()) {
      return;
   }

   selectedWallet_ = *walletsModel_->getWallet(index).ids.cbegin();
   QDialog::accept();
}
