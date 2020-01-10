/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "CommonMessageBoxDialog.h"

#include <QCoreApplication>
#include <QLayout>
#include <QApplication>

constexpr int MinimumWidth = 380;

CommonMessageBoxDialog::CommonMessageBoxDialog(QWidget* parent)
   : QDialog(parent)
{}

void CommonMessageBoxDialog::showEvent(QShowEvent *e)
{
   UpdateSize();
   QDialog::showEvent(e);
}

void CommonMessageBoxDialog::UpdateSize()
{
   if (!isVisible())
      return;

   layout()->activate();
   int width = layout()->totalMinimumSize().width();

   QFontMetrics fm(QApplication::font("QMdiSubWindowTitleBar"));
   int windowTitleWidth = fm.width(windowTitle()) + 50;
   if (windowTitleWidth > width) {
      width = windowTitleWidth;
   }

   if (width < MinimumWidth) {
      width = MinimumWidth;
   }

   layout()->activate();
   int height = (layout()->hasHeightForWidth())
      ? layout()->totalHeightForWidth(width)
      : layout()->totalMinimumSize().height();

   setFixedSize(width, height);
   QCoreApplication::removePostedEvents(this, QEvent::LayoutRequest);
}
