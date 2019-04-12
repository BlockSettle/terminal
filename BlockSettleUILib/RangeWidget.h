#ifndef __RANGE_WIDGET_H__
#define __RANGE_WIDGET_H__

#include <QWidget>
#include <memory>

namespace Ui {
   class RangeWidget;
};

class RangeWidget : public QWidget
{
public:
   RangeWidget(QWidget* parent = nullptr);
   ~RangeWidget() override;

   void SetRange(int lower, int upper);

   int GetLowerValue() const;
   int GetUpperValue() const;

private slots:
   void onLowerValueChanged(int newLower);
   void onUpperValueChanged(int newUpper);
private:
   std::unique_ptr<Ui::RangeWidget> ui_;
};

#endif // __RANGE_WIDGET_H__
