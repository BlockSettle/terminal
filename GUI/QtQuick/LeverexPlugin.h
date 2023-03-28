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

class LeverexPlugin: public Plugin
{
   Q_OBJECT
public:
   LeverexPlugin(QObject *parent);

   QString name() override { return QLatin1Literal("Leverex"); }
   QString description() override { return tr("Leverage made simple"); }
   QString icon() override { return QLatin1Literal("qrc:/images/leverex_plugin.png"); }
   QString path() override { return {}; }

   Q_INVOKABLE void init() override {}

private:
};
