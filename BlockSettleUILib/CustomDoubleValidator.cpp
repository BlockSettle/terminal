#include "CustomDoubleValidator.h"

#include "UiUtils.h"

CustomDoubleValidator::CustomDoubleValidator(QObject* parent)
   : QDoubleValidator(parent)
{
   setBottom(0.0);
   setNotation(QDoubleValidator::StandardNotation);
   setDecimals(0);
}

void CustomDoubleValidator::setDecimals(int decimals)
{
   decimals_ = decimals;
   QDoubleValidator::setDecimals(decimals_);
}

QValidator::State CustomDoubleValidator::validate(QString &input, int &pos) const
{
   return UiUtils::ValidateDoubleString(input, pos, decimals_);
}

double CustomDoubleValidator::GetValue(const QString& input, bool *ok) const
{
   QString tempCopy = UiUtils::NormalizeString(input);
   return QLocale().toDouble(tempCopy, ok);
}
