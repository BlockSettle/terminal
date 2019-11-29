/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/

#include "RangeSlider.h"

namespace
{
   const int scHandleSideLength = 11;
   const int scSliderBarHeight = 5;
   const int scLeftRightMargin = 1;
}


RangeSlider::RangeSlider(QWidget* parent)
   : QWidget(parent)
   , minimum_(0)
   , maximum_(100)
   , lowerValue_(0)
   , upperValue_(100)
   , slidingLower_(false)
   , slidingUpper_(false)
   , interval_(maximum_ - minimum_)
   , backgroudColorEnabled_(QColor(0x1E, 0x90, 0xFF))
   , backgroudColorDisabled_(Qt::darkGray)
   , backgroudColor_(backgroudColorEnabled_)
{
    setMouseTracking(true);
}

RangeSlider::~RangeSlider() noexcept = default;

void RangeSlider::paintEvent(QPaintEvent* )
{
   QPainter painter(this);

   // Background
   QRectF backgroundRect = QRectF(scLeftRightMargin, (height() - scSliderBarHeight) / 2, width() - scLeftRightMargin * 2, scSliderBarHeight);
   QPen pen(Qt::gray, 0.8);
   painter.setPen(pen);
   painter.setRenderHint(QPainter::Qt4CompatiblePainting);
   QBrush backgroundBrush(QColor(0xD0, 0xD0, 0xD0));
   painter.setBrush(backgroundBrush);
   painter.drawRoundedRect(backgroundRect, 1, 1);

   // First value handle rect
   pen.setColor(Qt::darkGray);
   pen.setWidth(0.5);
   painter.setPen(pen);
   painter.setRenderHint(QPainter::Antialiasing);
   QBrush handleBrush(QColor(0xFA, 0xFA, 0xFA));
   painter.setBrush(handleBrush);
   QRectF leftHandleRect = firstHandleRect();
   painter.drawRoundedRect(leftHandleRect, 2, 2);

   // Second value handle rect
   QRectF rightHandleRect = secondHandleRect();
   painter.drawRoundedRect(rightHandleRect, 2, 2);

   // Handles
   painter.setRenderHint(QPainter::Antialiasing, false);
   QRectF selectedRect(backgroundRect);
   selectedRect.setLeft(leftHandleRect.right() + 0.5);
   selectedRect.setRight(rightHandleRect.left() - 0.5);
   QBrush selectedBrush(backgroudColor_);
   painter.setBrush(selectedBrush);
   painter.drawRect(selectedRect);
}

QRectF RangeSlider::firstHandleRect() const
{
   float percentage = (lowerValue_ - minimum_) * 1.0 / interval_;
   return handleRect(percentage * validWidth() + scLeftRightMargin);
}

QRectF RangeSlider::secondHandleRect() const
{
   float percentage = (upperValue_ - minimum_) * 1.0 / interval_;
   return handleRect(percentage * validWidth() + scLeftRightMargin + scHandleSideLength);
}

QRectF RangeSlider::handleRect(int aValue) const
{
   return QRect(aValue, (height()-scHandleSideLength) / 2, scHandleSideLength, scHandleSideLength);
}

void RangeSlider::mousePressEvent(QMouseEvent* event)
{
   if (event->buttons() & Qt::LeftButton) {
      slidingUpper_ = secondHandleRect().contains(event->pos());
      slidingLower_ = !slidingUpper_ && firstHandleRect().contains(event->pos());

      if (slidingLower_) {
         delta_ = event->pos().x() - (firstHandleRect().x() + scHandleSideLength / 2);
      } else if (slidingUpper_) {
         delta_ = event->pos().x() - (secondHandleRect().x() + scHandleSideLength / 2);
      }

      if (   (event->pos().y() >= 2)
         && event->pos().y() <= height()- 2) {
         int step = interval_ / 10 < 1 ? 1 : interval_ / 10;
         if (event->pos().x() < firstHandleRect().x()) {
            setLowerValue(lowerValue_ - step);
         } else if ((event->pos().x() > firstHandleRect().x() + scHandleSideLength)
                  && event->pos().x() < secondHandleRect().x()) {
            if (event->pos().x() - (firstHandleRect().x() + scHandleSideLength) < (secondHandleRect().x() - (firstHandleRect().x() + scHandleSideLength)) / 2) {
               if (lowerValue_ + step < upperValue_) {
                  setLowerValue(lowerValue_ + step);
               } else {
                  setLowerValue(upperValue_);
               }
            } else {
               if (upperValue_ - step > lowerValue_) {
                  setUpperValue(upperValue_ - step);
               } else {
                  setUpperValue(lowerValue_);
               }
            }
         } else if (event->pos().x() > secondHandleRect().x() + scHandleSideLength) {
            setUpperValue(upperValue_ + step);
         }
      }
   }
}

void RangeSlider::mouseMoveEvent(QMouseEvent* event)
{
   if (event->buttons() & Qt::LeftButton) {
      if (slidingLower_) {
         if (event->pos().x() - delta_ + scHandleSideLength / 2 <= secondHandleRect().x()) {
            setLowerValue((event->pos().x() - delta_ - scLeftRightMargin - scHandleSideLength / 2) * 1.0 / validWidth() * interval_ + minimum_);
         } else {
            setLowerValue(upperValue_);
         }
      } else if (slidingUpper_) {
         if (firstHandleRect().x() + scHandleSideLength * 1.5 <= event->pos().x() - delta_) {
            setUpperValue((event->pos().x() - delta_ - scLeftRightMargin - scHandleSideLength / 2 - scHandleSideLength) * 1.0 / validWidth() * interval_ + minimum_);
         } else {
            setUpperValue(lowerValue_);
         }
      }
   }
}

void RangeSlider::mouseReleaseEvent(QMouseEvent*)
{
    slidingLower_ = false;
    slidingUpper_ = false;
}

void RangeSlider::changeEvent(QEvent* event)
{
   if (event->type() == QEvent::EnabledChange) {
      if (isEnabled()) {
         backgroudColor_ = backgroudColorEnabled_;
      } else {
         backgroudColor_ = backgroudColorDisabled_;
      }
      update();
   }
}

QSize RangeSlider::minimumSizeHint() const
{
   return QSize(scHandleSideLength * 2 + scLeftRightMargin * 2, scHandleSideLength);
}

int RangeSlider::GetMinimum() const
{
   return minimum_;
}

void RangeSlider::SetMinimum(int minimum)
{
   setMinimum(minimum);
}

int RangeSlider::GetMaximum() const
{
   return maximum_;
}

void RangeSlider::SetMaximum(int maximum)
{
   setMaximum(maximum);
}

int RangeSlider::GetLowerValue() const
{
   return lowerValue_;
}

void RangeSlider::SetLowerValue(int lowerValue)
{
    setLowerValue(lowerValue);
}

int RangeSlider::GetUpperValue() const
{
   return upperValue_;
}

void RangeSlider::SetUpperValue(int upperValue)
{
   setUpperValue(upperValue);
}

void RangeSlider::setLowerValue(int lowerValue)
{
   if (lowerValue > maximum_) {
      lowerValue = maximum_;
   }

   if (lowerValue < minimum_) {
      lowerValue = minimum_;
   }

   lowerValue_ = lowerValue;
   emit lowerValueChanged(lowerValue_);

   update();
}

void RangeSlider::setUpperValue(int upperValue)
{
   if (upperValue > maximum_) {
      upperValue = maximum_;
   }

   if (upperValue < minimum_) {
      upperValue = minimum_;
   }

   upperValue_ = upperValue;
   emit upperValueChanged(upperValue_);

   update();
   }

void RangeSlider::setMinimum(int minimum)
{
   if (minimum <= maximum_) {
      minimum_ = minimum;
   } else {
      int oldMax = maximum_;
      minimum_ = oldMax;
      maximum_ = minimum;
   }
   interval_ = maximum_ - minimum_;
   update();

   setLowerValue(minimum_);
   setUpperValue(maximum_);
}

void RangeSlider::setMaximum(int maximum)
{
   if (maximum >= minimum_)
   {
      maximum_ = maximum;
   } else {
      int oldMin = minimum_;
      maximum_ = oldMin;
      minimum_ = maximum;
   }

   interval_ = maximum_ - minimum_;
   update();

   setLowerValue(minimum_);
   setUpperValue(maximum_);
}

int RangeSlider::validWidth() const
{
   return width() - scLeftRightMargin * 2 - scHandleSideLength * 2;
}

void RangeSlider::SetRange(int minimum, int maximum)
{
   setMaximum(maximum);
   setMinimum(minimum);
}
