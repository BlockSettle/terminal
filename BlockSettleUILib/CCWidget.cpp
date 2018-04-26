#include "CCWidget.h"

#include "ui_CCWidget.h"

#include "CCPortfolioModel.h"
#include "UiUtils.h"
#include "AssetManager.h"

CCWidget::CCWidget(QWidget* parent)
   : QWidget(parent)
   , ui(new Ui::CCWidget())
{
   ui->setupUi(this);
}

void CCWidget::SetPortfolioModel(const std::shared_ptr<CCPortfolioModel>& model)
{
   assetManager_ = model->assetManager();

   ui->treeViewCC->setModel(model.get());
   ui->treeViewCC->header()->setSectionResizeMode(QHeaderView::Stretch);

   connect(model.get(), &CCPortfolioModel::rowsInserted, this, &CCWidget::onRowsInserted);
   connect(assetManager_.get(), &AssetManager::totalChanged, this, &CCWidget::updateTotalAssets);
   updateTotalAssets();
}

void CCWidget::updateTotalAssets()
{
   auto assets = assetManager_->getTotalAssets();
   if (assets < 0) {
      ui->labelTotalValue->setText(tr("<b>%1</b>").arg(tr("Loading...")));
   }
   else {
      ui->labelTotalValue->setText(tr("<b>%1</b>").arg(UiUtils::displayAmount(assets)));
   }
}

void CCWidget::onRowsInserted(const QModelIndex &parent, int first, int last)
{
    Q_UNUSED(first)
    Q_UNUSED(last)
   if (ui->treeViewCC->model()->data(parent) != UiUtils::XbtCurrency) {
      ui->treeViewCC->expand(parent);
   }
}
