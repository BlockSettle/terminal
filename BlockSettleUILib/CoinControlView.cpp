/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/

#include "CoinControlView.h"
#include "CoinControlModel.h"
#include "CoinControlWidget.h"

#include <QHeaderView>
#include <QStyle>
#include <QPaintEvent>


//
// CoinControlView
//

CoinControlView::CoinControlView(QWidget *parent)
   : QTreeView(parent)
   , model_(nullptr)
   , header_(nullptr)
   , currentPainted_(0)
{
   connect(this, &CoinControlView::expanded, this, &CoinControlView::onExpanded);
   connect(this, &CoinControlView::collapsed, this, &CoinControlView::onCollapsed);
}

void CoinControlView::setCoinsModel(CoinControlModel *model)
{
   connect(model, &CoinControlModel::rowsInserted, this, &CoinControlView::onRowsInserted);
   connect(model, &CoinControlModel::rowsRemoved, this, &CoinControlView::onRowsRemoved);

   model_ = model;

   calcCountOfVisibleRows();

   resizeColumnToContents(CoinControlModel::ColumnName);
   resizeColumnToContents(CoinControlModel::ColumnBalance);
   resizeColumnToContents(CoinControlModel::ColumnUTXOCount);
}

void CoinControlView::setCCHeader(CCHeader *header)
{
   header_ = header;
}

void CoinControlView::resizeEvent(QResizeEvent *e)
{
   QTreeView::resizeEvent(e);

   const int nameBalanceUtxoWidth = header()->sectionSize(CoinControlModel::ColumnName) +
      header()->sectionSize(CoinControlModel::ColumnBalance) +
      header()->sectionSize(CoinControlModel::ColumnUTXOCount) +
      (header_ ? header_->checkboxSizeHint().width() + 4 : 0);
   const int commentWidth = header()->width() - nameBalanceUtxoWidth -
      CoinControlModel::ColumnsCount + 1;

   header()->resizeSection(CoinControlModel::ColumnComment, commentWidth);

   if (header()->length() < header()->width()) {
      header()->resizeSection(CoinControlModel::ColumnBalance,
         header()->sectionSize(CoinControlModel::ColumnBalance) +
         header()->width() - header()->length());
   }
}

void CoinControlView::drawRow(QPainter *painter, const QStyleOptionViewItem &option,
   const QModelIndex &index) const
{
   if (index.parent().isValid()) {
      QStyleOptionViewItem opt = option;

      opt.features.setFlag(QStyleOptionViewItem::Alternate, currentPainted_ & 1);

      int left = header()->sectionViewportPosition(CoinControlModel::ColumnName) +
         indentation() * 2;
      int right = header()->sectionViewportPosition(CoinControlModel::ColumnBalance);

      opt.rect.setX(0);
      opt.rect.setWidth(right);

      QStyle::State oldState = opt.state;
      opt.state &= ~QStyle::State_Selected;
      style()->drawPrimitive(QStyle::PE_PanelItemViewRow, &opt, painter, this);
      opt.state = oldState;

      opt.rect.setX(left);
      opt.rect.setWidth(right - left);

      itemDelegate()->paint(painter, opt, index);

      left = header()->sectionViewportPosition(CoinControlModel::ColumnBalance);
      right = header()->sectionViewportPosition(CoinControlModel::ColumnBalance) +
         header()->sectionSize(CoinControlModel::ColumnBalance);

      opt.rect.setX(left);
      opt.rect.setWidth(right - left);

      opt.state &= ~QStyle::State_Selected;
      style()->drawPrimitive(QStyle::PE_PanelItemViewRow, &opt, painter, this);
      opt.state = oldState;

      itemDelegate()->paint(painter, opt, index.model()->index(
         index.row(), CoinControlModel::ColumnBalance, index.parent()));
   } else {
      QTreeView::drawRow(painter, option, index);
   }

   ++currentPainted_;
}

void CoinControlView::paintEvent(QPaintEvent *e)
{
   currentPainted_ = visibleRow(e->rect().topLeft());

   QTreeView::paintEvent(e);
}

int CoinControlView::visibleRow(const QPoint &p) const
{
   const QModelIndex index = indexAt(p);

   if (index.isValid()) {
      const auto it = visible_.find(index);

      if (it != visible_.cend()) {
         return it->second;
      } else {
         return -1;
      }
   } else {
      return -1;
   }
}

void CoinControlView::calcCountOfVisibleRows()
{
   visible_.clear();

   int r = 0;

   for (int i = 0; i < model_->rowCount(); ++i) {
      const QModelIndex idx = model_->index(i, 0);
      visible_[idx] = r;
      ++r;

      calcCountOfVisibleRows(idx, r);
   }
}

void CoinControlView::calcCountOfVisibleRows(const QModelIndex &parent, int &row)
{
   if (isExpanded(parent)) {
      for (int i = 0; i < model_->rowCount(parent); ++i) {
         const QModelIndex idx = model_->index(i, 0, parent);
         visible_[idx] = row;
         ++row;

         calcCountOfVisibleRows(idx, row);
      }
   }
}

void CoinControlView::onRowsInserted(const QModelIndex &, int, int)
{
   calcCountOfVisibleRows();
}

void CoinControlView::onRowsRemoved(const QModelIndex &, int, int)
{
   calcCountOfVisibleRows();
}

void CoinControlView::onCollapsed(const QModelIndex &)
{
   calcCountOfVisibleRows();
}

void CoinControlView::onExpanded(const QModelIndex &)
{
   calcCountOfVisibleRows();
}
