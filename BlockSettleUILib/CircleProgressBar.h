/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/

#ifndef CIRCLEPROGRESSBAR_H_INCLUDED
#define CIRCLEPROGRESSBAR_H_INCLUDED

#include <QWidget>


//
// CircleProgressBar
//

//! Circle progress bar.
class CircleProgressBar : public QWidget
{
   Q_OBJECT

   Q_PROPERTY(int minimum READ minimum WRITE setMinimum)
   Q_PROPERTY(int maximum READ maximum WRITE setMaximum)
   Q_PROPERTY(int value READ value WRITE setValue NOTIFY valueChanged)
   Q_PROPERTY(QColor color READ color WRITE setColor)

signals:
   void valueChanged(int);

public:
   CircleProgressBar(QWidget *parent = nullptr);
   ~CircleProgressBar() noexcept override = default;

   int minimum() const;
   void setMinimum(int v);

   int maximum() const;
   void setMaximum(int v);

   int value() const;
   void setValue(int v);

   const QColor& color() const;
   void setColor(const QColor &c);

   void setSize(const QSize &s);

   QSize minimumSizeHint() const override;
   QSize sizeHint() const override;

protected:
   void paintEvent(QPaintEvent*) override;

private:
   Q_DISABLE_COPY(CircleProgressBar)

   int min_;
   int max_;
   int value_;
   QColor color_;
   QSize size_;
}; // class CircleProgressBar

#endif // CIRCLEPROGRESSBAR_H_INCLUDED
