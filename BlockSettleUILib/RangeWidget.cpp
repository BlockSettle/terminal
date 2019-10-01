#include "RangeWidget.h"

#include "ui_RangeWidget.h"

#include "RangeSlider.h"

RangeWidget::RangeWidget(QWidget* parent)
   : QWidget{parent}
   , ui_{new Ui::RangeWidget{}}
{
   ui_->setupUi(this);

   connect(ui_->widgetRangeSlider, &RangeSlider::lowerValueChanged, this, &RangeWidget::onLowerValueChanged);
   connect(ui_->widgetRangeSlider, &RangeSlider::upperValueChanged, this, &RangeWidget::onUpperValueChanged);
}

RangeWidget::~RangeWidget() = default;


void RangeWidget::SetRange(int lower, int upper)
{
   ui_->widgetRangeSlider->SetRange(lower, upper);
}

int RangeWidget::GetLowerValue() const
{
   return ui_->widgetRangeSlider->GetLowerValue();
}

int RangeWidget::GetUpperValue() const
{
   return ui_->widgetRangeSlider->GetUpperValue();
}

void RangeWidget::SetLowerValue(int value)
{
   ui_->widgetRangeSlider->SetLowerValue(value);
}

void RangeWidget::SetUpperValue(int value)
{
   ui_->widgetRangeSlider->SetUpperValue(value);
}


void RangeWidget::onLowerValueChanged(int newLower)
{
   ui_->labelLower->setText(QString::number(newLower));
   emit lowerValueChanged(newLower);
}

void RangeWidget::onUpperValueChanged(int newUpper)
{
   ui_->labelUpper->setText(QString::number(newUpper));
   emit upperValueChanged(newUpper);
}
