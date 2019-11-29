/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "CustomComboBox.h"

#include <QListView>

CustomComboBox::CustomComboBox(QWidget *parent)
   : QComboBox(parent)
   , listView_(new QListView(this))
   , firstItemHidden_(false)
{
   setView(listView_.get());
}

CustomComboBox::~CustomComboBox() = default;

void CustomComboBox::showPopup()
{
   if (isFirstItemHidden()) {
      hideFirstItem();
   }

   QComboBox::showPopup();
   emit showPopupTriggered();
}

void CustomComboBox::hidePopup()
{
   if (isFirstItemHidden()) {
      showFirstItem();
   }

   QComboBox::hidePopup();
   emit hidePopupTriggered();
}

bool CustomComboBox::isFirstItemHidden() const
{
   return firstItemHidden_;
}

void CustomComboBox::setFirstItemHidden(bool firstItemHidden)
{
   if (firstItemHidden_ == firstItemHidden)
      return;

   firstItemHidden_ = firstItemHidden;
   emit firstItemHiddenChanged(firstItemHidden_);
}

void CustomComboBox::showFirstItem()
{
   if (count() > 0) {
      listView_->setRowHidden(0, false);
      style()->polish(listView_.get());
   }
}

void CustomComboBox::hideFirstItem()
{
   if (count() > 0) {
      listView_->setRowHidden(0, true);
      style()->polish(listView_.get());
   }
}
