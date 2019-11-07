#ifndef __CC_AMOUNT_VALIDATOR_H__
#define __CC_AMOUNT_VALIDATOR_H__

#include "CustomControls/CustomDoubleValidator.h"

class CCAmountValidator : public CustomDoubleValidator
{
Q_OBJECT

public:
   CCAmountValidator(QObject* parent);
   ~CCAmountValidator() noexcept override = default;
};

#endif // __CC_AMOUNT_VALIDATOR_H__
