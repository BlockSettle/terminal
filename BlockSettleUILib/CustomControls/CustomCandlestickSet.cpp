/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "CustomCandlestickSet.h"


CustomCandlestickSet::CustomCandlestickSet(qreal open, qreal high, qreal low, qreal close, qreal volume, qreal timestamp, QObject *parent)
   : QCandlestickSet(open, high, low, close, timestamp, parent)
   , volume_(volume) {

}
