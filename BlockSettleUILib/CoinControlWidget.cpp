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
// DelegateFor0Column
//

//! Delegate for the first (0) column.
class DelegateFor0Column Q_DECL_FINAL : public QStyledItemDelegate
{
public:
   explicit DelegateFor0Column(QObject *parent)
      : QStyledItemDelegate(parent)
      , mouseButtonPressed_(false)
   {
   }

   virtual ~DelegateFor0Column()
   {
   }

   bool editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option,
                    const QModelIndex &index) Q_DECL_OVERRIDE
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
}; // class DelegateFor0Column



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

void CoinControlWidget::rowClicked(const QModelIndex &index)
{
   if (index.isValid() && index.column() != 0) {
      const QModelIndex column0Index = coinControlModel_->index(index.row(), 0, index.parent());
      const int currentState = coinControlModel_->data(column0Index, Qt::CheckStateRole).toInt();
      coinControlModel_->setData(column0Index,
         (currentState == Qt::Unchecked ? Qt::Checked : Qt::Unchecked), Qt::CheckStateRole);
   }
}

void CoinControlWidget::initWidget(const std::shared_ptr<SelectedTransactionInputs>& selectedInputs)
{
   assert(selectedInputs != nullptr);
   ui_->checkBoxUseAllSelected->setChecked(selectedInputs->UseAutoSel());

   coinControlModel_ = new CoinControlModel(selectedInputs);
   ui_->treeViewUTXO->setModel(coinControlModel_);
   ui_->treeViewUTXO->setExpandsOnDoubleClick(false);
   ui_->treeViewUTXO->setItemDelegateForColumn(0, new DelegateFor0Column(ui_->treeViewUTXO));

   auto ccHeader = new CCHeader(selectedInputs->GetTotalTransactionsCount(), Qt::Horizontal, ui_->treeViewUTXO);
   ccHeader->setStretchLastSection(true);
   ui_->treeViewUTXO->setHeader(ccHeader);
   ui_->treeViewUTXO->header()->setSectionResizeMode(QHeaderView::ResizeToContents);

   connect(ccHeader, &CCHeader::stateChanged, coinControlModel_, &CoinControlModel::selectAll);
   connect(this, &CoinControlWidget::coinSelectionChanged, ccHeader, &CCHeader::onSelectionChanged);

   connect(coinControlModel_, &CoinControlModel::selectionChanged, this, &CoinControlWidget::updateSelectedTotals);
   connect(ui_->checkBoxUseAllSelected, &QCheckBox::stateChanged, this, &CoinControlWidget::onAutoSelClicked);
   onAutoSelClicked(selectedInputs->UseAutoSel() ? Qt::Checked : Qt::Unchecked);

   connect(ui_->treeViewUTXO, &QTreeView::clicked, this, &CoinControlWidget::rowClicked);
}

void CoinControlWidget::applyChanges(const std::shared_ptr<SelectedTransactionInputs>& selectedInputs)
{
   selectedInputs->SetUseAutoSel(ui_->checkBoxUseAllSelected->isChecked());
   coinControlModel_->ApplySelection(selectedInputs);
}
