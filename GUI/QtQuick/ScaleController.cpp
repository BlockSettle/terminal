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
#include <QDesktopWidget>

namespace {
   const qreal defaultDpi = 96;
}

ScaleController::ScaleController(QObject* parent)
   : QObject(parent)
{
	update();
}

void ScaleController::update()
{
   disconnect(this);

   const auto screen = QGuiApplication::screens()[QApplication::desktop()->screenNumber(QApplication::activeWindow())];
   connect(screen, &QScreen::logicalDotsPerInchChanged, this, &ScaleController::update);

   qreal dpi = screen->logicalDotsPerInch();
   scaleRatio_ = dpi / defaultDpi;

   QRect rect = screen->geometry();
   screenWidth_ = rect.width();
   screenHeight_ = rect.height();

   emit changed();
}