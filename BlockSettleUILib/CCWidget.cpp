/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "CCWidget.h"

#include "ui_CCWidget.h"

#include "CCPortfolioModel.h"
#include "UiUtils.h"
#include "AssetManager.h"
#include "Wallets/SyncWalletsManager.h"

CCWidget::CCWidget(QWidget* parent)
   : QWidget(parent)
   , ui_(new Ui::CCWidget())
{
   ui_->setupUi(this);
}

CCWidget::~CCWidget() = default;

void CCWidget::SetPortfolioModel(const std::shared_ptr<CCPortfolioModel>& model)
{
   assetManager_ = model->assetManager();
   const auto &walletsManager = model->walletsManager();

   ui_->treeViewCC->setModel(model.get());
   ui_->treeViewCC->header()->setSectionResizeMode(QHeaderView::Stretch);

   connect(model.get(), &CCPortfolioModel::rowsInserted, this, &CCWidget::onRowsInserted);
   connect(assetManager_.get(), &AssetManager::totalChanged, this, &CCWidget::updateTotalAssets);
   connect(walletsManager.get(), &bs::sync::WalletsManager::walletBalanceUpdated, this, &CCWidget::updateTotalAssets);
   updateTotalAssets();
}

void CCWidget::updateTotalAssets()
{
   auto assets = assetManager_->getTotalAssets();
   if (assets < 0) {
      ui_->labelTotalValue->setText(tr("<b>%1</b>").arg(tr("Loading...")));
   }
   else {
      ui_->labelTotalValue->setText(tr("<b>%1</b>").arg(UiUtils::displayAmount(assets)));
   }
}

void CCWidget::onRowsInserted(const QModelIndex &parent, int first, int last)
{
    Q_UNUSED(first)
    Q_UNUSED(last)
   if (ui_->treeViewCC->model()->data(parent) != UiUtils::XbtCurrency) {
      ui_->treeViewCC->expand(parent);
   }
}
