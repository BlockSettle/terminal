#include "CoinControlWidget.h"
#include "ui_CoinControlWidget.h"

#include "UiUtils.h"
#include "CoinControlModel.h"
#include "SelectedTransactionInputs.h"


CoinControlWidget::CoinControlWidget(QWidget* parent)
   : QWidget(parent)
   , ui_(new Ui::CoinControlWidget())
{
   ui_->setupUi(this);
   ui_->checkBoxUseAllSelected->setText(tr("Auto-select Inputs"));
   ui_->treeViewUTXO->setUniformRowHeights(true);
}

void CoinControlWidget::updateSelectedTotals()
{
   ui_->labelTotalAmount->setText(coinControlModel_->GetSelectedBalance());
   ui_->labelTotalTransactions->setText(QString::number(coinControlModel_->GetSelectedTransactionsCount()));
   ui_->checkBoxUseAllSelected->setChecked(false);
   emit coinSelectionChanged(coinControlModel_->GetSelectedTransactionsCount());
}

void CoinControlWidget::onAutoSelClicked(int state)
{
   if (state == Qt::Checked) {
	  ui_->labelTotalAmount->setText(coinControlModel_->GetTotalBalance());
	  ui_->labelTotalTransactions->clear();
	  emit coinSelectionChanged(MAXSIZE_T);
   }
   else {
	  updateSelectedTotals();
   }
}

void CoinControlWidget::initWidget(const std::shared_ptr<SelectedTransactionInputs>& selectedInputs)
{
   assert(selectedInputs != nullptr);
   ui_->checkBoxUseAllSelected->setChecked(selectedInputs->UseAutoSel());

   coinControlModel_ = new CoinControlModel(selectedInputs);
   /*coinControlModelProxy_ = new SortingCoinControlModel(this);
   coinControlModelProxy_->setSourceModel(coinControlModel_);
   coinControlModelProxy_->setDynamicSortFilter(true);
   ui_->treeViewUTXO->setModel(coinControlModelProxy_);
   */
   ui_->treeViewUTXO->setModel(coinControlModel_);
   auto ccHeader = new CCHeader(selectedInputs->GetTotalTransactionsCount(), Qt::Horizontal, ui_->treeViewUTXO);
   ccHeader->setStretchLastSection(true);
   ccHeader->setSectionsClickable(true);
   ui_->treeViewUTXO->setSortingEnabled(true);
   ui_->treeViewUTXO->setHeader(ccHeader);
   ui_->treeViewUTXO->header()->setSectionResizeMode(QHeaderView::ResizeToContents);

   connect(ccHeader, &CCHeader::stateChanged, coinControlModel_, &CoinControlModel::selectAll);
   connect(this, &CoinControlWidget::coinSelectionChanged, ccHeader, &CCHeader::onSelectionChanged);
   auto sigCon = connect(ccHeader, SIGNAL(sectionClicked(int)), ui_->treeViewUTXO, SLOT(sortByColumn(int)));

   connect(coinControlModel_, &CoinControlModel::selectionChanged, this, &CoinControlWidget::updateSelectedTotals);
   connect(ui_->checkBoxUseAllSelected, &QCheckBox::stateChanged, this, &CoinControlWidget::onAutoSelClicked);
   onAutoSelClicked(selectedInputs->UseAutoSel() ? Qt::Checked : Qt::Unchecked);
}

void CoinControlWidget::applyChanges(const std::shared_ptr<SelectedTransactionInputs>& selectedInputs)
{
   selectedInputs->SetUseAutoSel(ui_->checkBoxUseAllSelected->isChecked());
   coinControlModel_->ApplySelection(selectedInputs);
}
