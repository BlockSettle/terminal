/*

***********************************************************************************
* Copyright (C) 2023, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "SideShiftController.h"

SideShiftController::SideShiftController()
   : QObject()
{
}

QString SideShiftController::conversionRate() const
{
   return tr("1 BTC = 1 ETC");
}
