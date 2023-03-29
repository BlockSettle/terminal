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

   QString name() const override { return QLatin1Literal("Leverex"); }
   QString description() const override { return tr("Leverage made simple"); }
   QString icon() const override { return QLatin1Literal("qrc:/images/leverex_plugin.png"); }
   QString path() const override { return {}; }

   Q_INVOKABLE void init() override {}

private:
};
