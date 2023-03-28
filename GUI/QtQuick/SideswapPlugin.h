/*

***********************************************************************************
* Copyright (C) 2023, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#pragma once

#include "Plugin.h"

class SideswapPlugin: public Plugin
{
   Q_OBJECT
public:
   SideswapPlugin(QObject *parent);

   QString name() override { return QLatin1Literal("SideSwap.io"); }
   QString description() override { return tr("Easiest way to get started on the Liquid Network"); }
   QString icon() override { return QLatin1Literal("qrc:/images/sideswap_plugin.png"); }
   QString path() override { return QLatin1Literal("qrc:/qml/Plugins/SideSwap/SideSwapPopup.qml"); }

   Q_INVOKABLE void init() override {}

private:
};
