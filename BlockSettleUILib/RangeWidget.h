#ifndef __RANGE_WIDGET_H__
#define __RANGE_WIDGET_H__

#include <QWidget>
#include <memory>

namespace Ui {
   class RangeWidget;
};

class RangeWidget : public QWidget
{
   Q_OBJECT
public:
   RangeWidget(QWidget* parent = nullptr);
   ~RangeWidget() override;

   void SetRange(int lower, int upper);

   int GetLowerValue() const;
   int GetUpperValue() const;

   void SetLowerValue(int value);
   void SetUpperValue(int value);

private slots:
   void onLowerValueChanged(int newLower);
   void onUpperValueChanged(int newUpper);

signals:
   void lowerValueChanged(int lowerValue);
   void upperValueChanged(int upperValue);

private:
   std::unique_ptr<Ui::RangeWidget> ui_;
};

#endif // __RANGE_WIDGET_H__
