/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
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
