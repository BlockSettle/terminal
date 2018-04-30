#ifndef __CUSTOM_DOUBLE_SPIN_BOX_H__
#define __CUSTOM_DOUBLE_SPIN_BOX_H__

#include <QDoubleSpinBox>

class CustomDoubleSpinBox : public QDoubleSpinBox
{
public:
   explicit CustomDoubleSpinBox(QWidget *parent = nullptr);
   ~CustomDoubleSpinBox() override = default;

public:
   QValidator::State validate(QString &input, int &pos) const override;
   double valueFromText(const QString &text) const override;
   QString textFromValue(double val) const override;
   void fixup(QString &str) const override;
};

#endif // __CUSTOM_DOUBLE_SPIN_BOX_H__