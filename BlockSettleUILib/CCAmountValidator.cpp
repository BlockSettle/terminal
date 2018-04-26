#include "CCAmountValidator.h"

#include "UiUtils.h"

CCAmountValidator::CCAmountValidator(QObject* parent)
   : CustomDoubleValidator(parent)
{
   setBottom(0.0);
   setDecimals(UiUtils::GetAmountPrecisionCC());
}