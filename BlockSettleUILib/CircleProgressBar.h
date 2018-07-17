
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

   Q_PROPERTY(int min READ min WRITE setMin)
   Q_PROPERTY(int max READ max WRITE setMax)
   Q_PROPERTY(int value READ value WRITE setValue NOTIFY valueChanged)
   Q_PROPERTY(QColor color READ color WRITE setColor)

signals:
   void valueChanged(int);

public:
   CircleProgressBar(QWidget *parent = nullptr);
   ~CircleProgressBar() noexcept override = default;

   int min() const;
   void setMin(int v);

   int max() const;
   void setMax(int v);

   int value() const;
   void setValue(int v);

   const QColor& color() const;
   void setColor(const QColor &c);

   void setSize(const QSize &s);

   QSize minimumSizeHint() const override;
   QSize sizeHint() const override;

protected:
   void paintEvent(QPaintEvent*);

private:
   Q_DISABLE_COPY(CircleProgressBar)

   int min_;
   int max_;
   int value_;
   QColor color_;
   QSize size_;
}; // class CircleProgressBar

#endif // CIRCLEPROGRESSBAR_H_INCLUDED
