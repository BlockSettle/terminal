/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
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
