#include "CoinControlWidget.h"
#include "ui_CoinControlWidget.h"

#include "UiUtils.h"
#include "CoinControlModel.h"
#include "SelectedTransactionInputs.h"

#include <QTreeView>
#include <QStyledItemDelegate>
#include <QStyle>
#include <QApplication>
#include <QStyleOptionViewItem>


//
// DelegateForNameColumn
//

//! Delegate for the first (0) column with names, adresses.
class DelegateForNameColumn final : public QStyledItemDelegate
{
public:
   explicit DelegateForNameColumn(QObject *parent)
      : QStyledItemDelegate(parent)
      , mouseButtonPressed_(false)
   {
   }

   ~DelegateForNameColumn() noexcept override = default;

   bool editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option,
                    const QModelIndex &index) override
   {
      if (index.isValid()) {
         switch (event->type()) {
            case QEvent::MouseButtonPress :
            {
               QMouseEvent *e = static_cast<QMouseEvent*>(event);

               if (e->button() == Qt::LeftButton && option.rect.contains(e->pos())) {
                  mouseButtonPressed_ = true;

                  return false;
               }
            }
               break;

            case QEvent::MouseButtonRelease :
            {
               QMouseEvent *e = static_cast<QMouseEvent*>(event);

               if (e->button() == Qt::LeftButton && option.rect.contains(e->pos())
                  && mouseButtonPressed_) {
                  mouseButtonPressed_ = false;

                  const int currentState = model->data(index, Qt::CheckStateRole).toInt();
                  model->setData(index, (currentState == Qt::Unchecked ?
                                             Qt::Checked : Qt::Unchecked),
                                 Qt::CheckStateRole);

                  return false;
               } else {
                  mouseButtonPressed_ = false;
               }
            }
               break;

            default :
               break;
         }
      }

      return QStyledItemDelegate::editorEvent(event, model, option, index);
   }

private:
   bool mouseButtonPressed_;
}; // class DelegateForNameColumn


CoinControlWidget::CoinControlWidget(QWidget* parent)
   : QWidget(parent)
   , ui_(new Ui::CoinControlWidget())
{
   ui_->setupUi(this);
   ui_->checkBoxUseAllSelected->setText(tr("Auto-select Inputs"));
   ui_->treeViewUTXO->setUniformRowHeights(true);
}

CoinControlWidget::~CoinControlWidget() = default;

void CoinControlWidget::updateSelectedTotals()
{
   ui_->labelTotalAmount->setText(coinControlModel_->GetSelectedBalance());
   ui_->labelTotalTransactions->setText(QString::number(coinControlModel_->GetSelectedTransactionsCount()));
   ui_->checkBoxUseAllSelected->setChecked(false);
   emit coinSelectionChanged(coinControlModel_->GetSelectedTransactionsCount(), false);
}

void CoinControlWidget::onAutoSelClicked(int state)
{
   if (state == Qt::Checked) {
      ui_->labelTotalAmount->setText(coinControlModel_->GetTotalBalance());
      ui_->labelTotalTransactions->clear();
      coinControlModel_->clearSelection();
      emit coinSelectionChanged(0, true);
   }
   else {
      ui_->labelTotalAmount->setText(coinControlModel_->GetSelectedBalance());
      ui_->labelTotalTransactions->setText(QString::number(coinControlModel_->GetSelectedTransactionsCount()));
      emit coinSelectionChanged(coinControlModel_->GetSelectedTransactionsCount(), false);
   }
}

void CoinControlWidget::rowClicked(const QModelIndex &index)
{
   if (index.isValid() && index.column() != 0) {
      const QModelIndex column0Index = coinControlModel_->index(index.row(), 0, index.parent());
      const int currentState = coinControlModel_->data(column0Index, Qt::CheckStateRole).toInt();
      coinControlModel_->setData(column0Index,
         (currentState == Qt::Unchecked ? Qt::Checked : Qt::Unchecked), Qt::CheckStateRole);
   }
}

void CoinControlWidget::initWidget(const std::shared_ptr<SelectedTransactionInputs>& selectedInputs, bool allowAutoSel)
{
   assert(selectedInputs != nullptr);

   coinControlModel_ = new CoinControlModel(selectedInputs);
   ui_->treeViewUTXO->setModel(coinControlModel_);
   ui_->treeViewUTXO->setExpandsOnDoubleClick(false);
   ui_->treeViewUTXO->setItemDelegateForColumn(0, new DelegateForNameColumn(ui_->treeViewUTXO));

   auto ccHeader = new CCHeader(selectedInputs->GetTotalTransactionsCount(), Qt::Horizontal, ui_->treeViewUTXO);
   ccHeader->setSectionsClickable(true);
   ui_->treeViewUTXO->setSortingEnabled(true);
   ui_->treeViewUTXO->setHeader(ccHeader);

   if (!allowAutoSel) {
      ui_->checkBoxUseAllSelected->setChecked(false);
   }
   ui_->checkBoxUseAllSelected->setEnabled(allowAutoSel);

   connect(ccHeader, &CCHeader::stateChanged, coinControlModel_, &CoinControlModel::selectAll);
   connect(this, &CoinControlWidget::coinSelectionChanged, ccHeader, &CCHeader::onSelectionChanged);
   connect(ccHeader, SIGNAL(sectionClicked(int)), ui_->treeViewUTXO, SLOT(sortByColumn(int)));

   connect(coinControlModel_, &CoinControlModel::selectionChanged, this, &CoinControlWidget::updateSelectedTotals);
   connect(ui_->checkBoxUseAllSelected, &QCheckBox::stateChanged, this, &CoinControlWidget::onAutoSelClicked);

   connect(ui_->treeViewUTXO, &QTreeView::clicked, this, &CoinControlWidget::rowClicked);

   ui_->treeViewUTXO->setCoinsModel(coinControlModel_);
   ui_->treeViewUTXO->setCCHeader(ccHeader);

   ui_->checkBoxUseAllSelected->setChecked(selectedInputs->UseAutoSel());
   onAutoSelClicked(selectedInputs->UseAutoSel() ? Qt::Checked : Qt::Unchecked);

   ui_->treeViewUTXO->sortByColumn(CoinControlModel::ColumnName, Qt::AscendingOrder);
}

void CoinControlWidget::applyChanges(const std::shared_ptr<SelectedTransactionInputs>& selectedInputs)
{
   selectedInputs->SetUseAutoSel(ui_->checkBoxUseAllSelected->isChecked());
   coinControlModel_->ApplySelection(selectedInputs);
}
