/*
***********************************************************************************
* Copyright (C) 2023, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************
*/
#pragma once

#include <QObject>
#include "Message/Adapter.h"

class Plugin: public QObject
{
   Q_OBJECT
public:
   Plugin(QObject *parent);

   virtual QString name() = 0;
   virtual QString description() = 0;
   virtual QString icon() = 0;
   virtual QString path() = 0;

   Q_INVOKABLE virtual void init() = 0;

private:
};
