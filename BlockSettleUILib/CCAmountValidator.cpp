/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "CCAmountValidator.h"

#include "UiUtils.h"

CCAmountValidator::CCAmountValidator(QObject* parent)
   : CustomDoubleValidator(parent)
{
   setBottom(0.0);
   setDecimals(UiUtils::GetAmountPrecisionCC());
}