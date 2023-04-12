/*

***********************************************************************************
* Copyright (C) 2023, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "ScaleController.h"
#include <QApplication>
#include <QScreen>

namespace {
   const qreal defaultDpi = 96;
}

ScaleController::ScaleController(QObject* parent)
   : QObject(parent)
{
   qreal dpi = QGuiApplication::primaryScreen()->logicalDotsPerInch();
   scaleRatio_ = dpi / defaultDpi;

   QRect rect = QGuiApplication::primaryScreen()->geometry();
   screenWidth_ = rect.width();
   screenHeight_ = rect.height();
}
