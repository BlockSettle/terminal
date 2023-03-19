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

class SideShiftController: public QObject
{
   Q_OBJECT
   Q_PROPERTY(QString conversionRate READ conversionRate NOTIFY changed)

public:
   SideShiftController();

   QString conversionRate() const;

signals:
   void changed();
};
