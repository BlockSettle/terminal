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

class SideshiftPlugin: public Plugin
{
   Q_OBJECT
public:
   SideshiftPlugin(QObject *parent);

   QString name() override { return QLatin1Literal("SideShift.ai"); }
   QString description() override { return tr("Shift between BTC, ETH, BCH, XMR, USDT and 90+ other cryptocurrencies"); }
   QString icon() override { return QLatin1Literal("qrc:/images/sideshift_plugin.png"); }
   QString path() override { return QLatin1Literal("qrc:/qml/Plugins/SideShift/SideShiftPopup.qml"); }

   Q_INVOKABLE void init() override;

private:
};
