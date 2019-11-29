/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "FXAmountValidator.h"

#include "UiUtils.h"

FXAmountValidator::FXAmountValidator(QObject* parent)
   : CustomDoubleValidator(parent)
{
   setBottom(0.0);
   setNotation(QDoubleValidator::StandardNotation);
   setDecimals(UiUtils::GetAmountPrecisionFX());
}
