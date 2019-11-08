#ifndef __FX_AMOUNT_VALIDATOR_H__
#define __FX_AMOUNT_VALIDATOR_H__

#include "CustomControls/CustomDoubleValidator.h"

class FXAmountValidator : public CustomDoubleValidator
{
Q_OBJECT

public:
   FXAmountValidator(QObject* parent);
   ~FXAmountValidator() noexcept override = default;
};

#endif // __FX_AMOUNT_VALIDATOR_H__
