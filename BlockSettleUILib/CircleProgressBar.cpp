/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/

#include "CircleProgressBar.h"

#include <QPainter>


//
// CircleProgressBar
//

CircleProgressBar::CircleProgressBar(QWidget *parent)
   : QWidget(parent)
   , min_(0)
   , max_(100)
   , value_(0)
   , color_(0x81, 0x88, 0x8F)
   , size_(16, 16)
{
   setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
   setAutoFillBackground(false);

   resize(size_);
}

int CircleProgressBar::minimum() const
{
   return min_;
}

void CircleProgressBar::setMinimum(int v)
{
   min_ = v;

   if (min_ > max_) {
      max_ = min_;
   }
}

int CircleProgressBar::maximum() const
{
   return max_;
}

void CircleProgressBar::setMaximum(int v)
{
   max_ = v;

   if (max_ < min_) {
      min_ = max_;
   }
}

int CircleProgressBar::value() const
{
   return value_;
}

void CircleProgressBar::setValue(int v)
{
   if (v < min_) {
      v = min_;
   } else if (v > max_) {
      v = max_;
   }

   value_ = v;

   emit valueChanged(value_);
}

const QColor& CircleProgressBar::color() const
{
   return color_;
}

void CircleProgressBar::setColor(const QColor &c)
{
   color_ = c;

   update();
}

void CircleProgressBar::setSize(const QSize &s)
{
   size_ = s;

   resize(size_);
}

QSize CircleProgressBar::minimumSizeHint() const
{
   return size_;
}

QSize CircleProgressBar::sizeHint() const
{
   return size_;
}

void CircleProgressBar::paintEvent(QPaintEvent*)
{
   QPainter p(this);
   p.setRenderHint(QPainter::Antialiasing);

   p.setPen(color_);
   p.setBrush(Qt::NoBrush);

   QRect r(1, 1, width() - 2, height() - 2);

   p.drawEllipse(r);

   p.setBrush(color_);

   const int span = ((qreal)(value_ - min_) / (qreal)(max_ - min_)) * 360 * 16;

   p.drawPie(r, 90 * 16, -span);
}
