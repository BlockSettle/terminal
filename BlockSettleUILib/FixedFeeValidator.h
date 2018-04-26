#ifndef __FIXED_FEE_VALIDATOR_H__
#define __FIXED_FEE_VALIDATOR_H__

#include <QIntValidator>
#include <QString>

class QComboBox;
class QLineEdit;

class FixedFeeValidator : public QIntValidator
{
Q_OBJECT

public:
   FixedFeeValidator(uint64_t initialFee, const QString& feeSuffix, QComboBox *parent);
   ~FixedFeeValidator() noexcept override = default;

   State validate(QString &input, int &pos) const override;
   void fixup(QString &input) const override;

   void setMinValue(float value) { minValue_ = value; }

public slots:
   void onCursorPositionChanged(int oldPosition, int newPosition);

signals:
   void feeUpdated(int fee) const;

private:
   void ConnectToComboBox(QComboBox *comboBox, uint64_t initialFee);
   float getNumber(const QString &input) const;

private:
   QString     feeSuffix_;
   QLineEdit   *lineEdit_;
   float       minValue_ = 0;
};


#endif // __FIXED_FEE_VALIDATOR_H__
