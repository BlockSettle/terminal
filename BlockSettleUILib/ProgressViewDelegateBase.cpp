/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "ProgressViewDelegateBase.h"
#include <QStyleOptionProgressBar>
#include <QApplication>

namespace {
   int const kMaxWidth = 100;
}

ProgressViewDelegateBase::ProgressViewDelegateBase(QWidget* parent)
   : QStyledItemDelegate(parent)
{
   pbar_.setStyleSheet(QLatin1String("QProgressBar { border: 1px solid #1c2835; "
      "border-radius: 4px; background-color: rgba(0, 0, 0, 0); }"));
   pbar_.hide();
}

void ProgressViewDelegateBase::paint(QPainter* painter, const QStyleOptionViewItem& opt, const QModelIndex& index) const
{
   if (isDrawProgressBar(index)) {
      QStyleOptionProgressBar pOpt;
      pOpt.maximum = maxValue(index);
      pOpt.minimum = 0;
      pOpt.progress = currentValue(index);
      pOpt.rect = opt.rect;
      pOpt.rect.setWidth(std::min(pOpt.rect.width(), kMaxWidth));

      QApplication::style()->drawControl(QStyle::CE_ProgressBar, &pOpt, painter, &pbar_);
   }
   else {
      QStyleOptionViewItem changedOpt = opt;
      changedOpt.state &= ~(QStyle::State_Selected);

      QStyledItemDelegate::paint(painter, changedOpt, index);
   }
}
