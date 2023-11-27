/*

***********************************************************************************
* Copyright (C) 2023, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "SideswapPlugin.h"
#include <qqml.h>

SideswapPlugin::SideswapPlugin(QObject* parent)
   : Plugin(parent)
{
   qmlRegisterInterface<SideswapPlugin>("SideswapPlugin");
}
