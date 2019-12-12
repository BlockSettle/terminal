/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "ToggleSwitch.h"
#include <QPainter>
#include <QVariant>
#include <QStyle>
#include <QPropertyAnimation>
#include <QMouseEvent>

///////////////////////////////////////////////////////////
// ToggleSwitch class implements toggle style control
// similar to the one used on iPhones. This class 
// inherits QSlider and requires the widget in the .ui
// file to also be of type QSlider. 
// How to use:
// - in Ui file add a horizontal slider control into your page
// - set maximumSize Width to 35 (can be tweaked if needed)
// - set minimumSize Height to 18
// - set QAbstractSlider minimum and maximum to 0 and 50
// - set singleStep and pageStep to 1
// - Promote your horizontal slider to ToggleSwitch
// 

ToggleSwitch::ToggleSwitch(QWidget *parent) 
   : QSlider(parent) {
   animation_ = new QPropertyAnimation(this, "value", this);
   checkState_ = Qt::Unchecked;
}

void ToggleSwitch::mousePressEvent(QMouseEvent *ev) {
   //qDebug() << "mousePressEvent";
   if (ev->button() == Qt::LeftButton) {
      setCheckState(property("toggleOn").toBool() ? Qt::Unchecked : Qt::Checked);
      emit clicked();
   }
   else if (ev->button() == Qt::RightButton) {
      //setCheckState(Qt::PartiallyChecked);
   }
}

void ToggleSwitch::setCheckState(Qt::CheckState state) {
   //qDebug() << "ToggleSwitch::setChecked" << state;
   this->setProperty("toggleOn", state == Qt::Checked ? true : false);
   style()->unpolish(this);
   style()->polish(this);
   animation_->setDuration(100);
   if (state == Qt::Checked) {
      animation_->setStartValue(value());
      animation_->setEndValue(maximum());
   }
   else if (state == Qt::PartiallyChecked) {
      animation_->setStartValue(value());
      animation_->setEndValue((maximum() - minimum()) / 2);
   }
   else {
      animation_->setStartValue(value());
      animation_->setEndValue(minimum());
   }
   animation_->start();
   checkState_ = state;
   emit stateChanged(checkState_);
}

void ToggleSwitch::setEnabled(bool bEnabled) {
   this->setProperty("enabled", bEnabled);
   style()->unpolish(this);
   style()->polish(this);
   //qDebug() << "ToggleSwitch::setEnabled" << bEnabled;
}