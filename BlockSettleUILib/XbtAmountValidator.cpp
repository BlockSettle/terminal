#include "XbtAmountValidator.h"

#include "UiUtils.h"

XbtAmountValidator::XbtAmountValidator(QObject* parent)
   : CustomDoubleValidator(parent)
{
   setBottom(0.0);
   setNotation(QDoubleValidator::StandardNotation);
   setDecimals(UiUtils::GetAmountPrecisionXBT());
}
