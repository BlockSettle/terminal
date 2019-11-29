/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __XBT_AMOUNT_VALIDATOR_H__
#define __XBT_AMOUNT_VALIDATOR_H__

#include "CustomControls/CustomDoubleValidator.h"

class XbtAmountValidator : public CustomDoubleValidator
{
Q_OBJECT

public:
   XbtAmountValidator(QObject* parent);
   ~XbtAmountValidator() noexcept override = default;
};


#endif // __XBT_AMOUNT_VALIDATOR_H__
