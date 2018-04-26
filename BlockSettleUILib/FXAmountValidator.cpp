#include "FXAmountValidator.h"

#include "UiUtils.h"

FXAmountValidator::FXAmountValidator(QObject* parent)
   : CustomDoubleValidator(parent)
{
   setBottom(0.0);
   setNotation(QDoubleValidator::StandardNotation);
   setDecimals(UiUtils::GetAmountPrecisionFX());
}
