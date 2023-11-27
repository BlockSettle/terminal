/*

***********************************************************************************
* Copyright (C) 2023, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "LeverexPlugin.h"
#include <qqml.h>

LeverexPlugin::LeverexPlugin(QObject* parent)
   : Plugin(parent)
{
   qmlRegisterInterface<LeverexPlugin>("LeverexPlugin");
}
