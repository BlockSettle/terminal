/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#pragma once

#include <QWidget>
#include <QPainter>
#include <QMouseEvent>

class RangeSlider : public QWidget
{
Q_OBJECT

public:
   RangeSlider(QWidget* parent = nullptr);
   ~RangeSlider() noexcept;

   RangeSlider(const RangeSlider&) = delete;
   RangeSlider& operator = (const RangeSlider&) = delete;

   RangeSlider(RangeSlider&&) = delete;
   RangeSlider& operator = (RangeSlider&&) = delete;
   QSize minimumSizeHint() const override;

   int GetMinimum() const;
   void SetMinimum(int minimum);

   int GetMaximum() const;
   void SetMaximum(int maximum);

   int GetLowerValue() const;
   void SetLowerValue(int aLowerValue);

   int GetUpperValue() const;
   void SetUpperValue(int aUpperValue);

   void SetRange(int minimum, int maximum);

protected:
   void paintEvent(QPaintEvent* event) override;
   void mousePressEvent(QMouseEvent* event) override;
   void mouseMoveEvent(QMouseEvent* event) override;
   void mouseReleaseEvent(QMouseEvent* event) override;
   void changeEvent(QEvent* event) override;

   QRectF firstHandleRect() const;
   QRectF secondHandleRect() const;
   QRectF handleRect(int value) const;

signals:
   void lowerValueChanged(int lowerValue);
   void upperValueChanged(int upperValue);

public slots:
   void setLowerValue(int lowerValue);
   void setUpperValue(int upperValue);
   void setMinimum(int minimum);
   void setMaximum(int maximum);

private:
   float currentPercentage();
   int validWidth() const;

private:
   int     minimum_;
   int     maximum_;
   int     lowerValue_;
   int     upperValue_;
   bool    slidingLower_;
   bool    slidingUpper_;
   int     interval_;
   int     delta_;

   QColor backgroudColorEnabled_;
   QColor backgroudColorDisabled_;
   QColor backgroudColor_;
};
