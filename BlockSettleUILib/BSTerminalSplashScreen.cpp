/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "BSTerminalSplashScreen.h"

#include <QBitmap>
#include <QApplication>


BSTerminalSplashScreen::BSTerminalSplashScreen(const QPixmap& splash_image)
   : QSplashScreen(splash_image)
{
   setMask(splash_image.mask());

   progress_ = new QProgressBar(this);
   progress_->setMinimum(0);
   progress_->setMaximum(100);
   progress_->setValue(0);
   progress_->setMinimumWidth(this->width());
   progress_->setMaximumHeight(10);

   blockSettleLabel_ = new QLabel(this);
   blockSettleLabel_->setText(QLatin1String("BLOCKSETTLE TERMINAL"));
   blockSettleLabel_->move(30, 140);
   blockSettleLabel_->setStyleSheet(QLatin1String("font-size: 18px; color: white"));

   progress_->setStyleSheet(QLatin1String("text-align: center; font-size: 8px; border-width: 0px;"));
   SetTipText(tr("Loading"));
}

BSTerminalSplashScreen::~BSTerminalSplashScreen() = default;

void BSTerminalSplashScreen::SetTipText(const QString& tip)
{
   progress_->setFormat(tip + QString::fromStdString(" %p%"));
   QApplication::processEvents();
}

void BSTerminalSplashScreen::SetProgress(int progressValue)
{
   progress_->setValue(progressValue);
   QApplication::processEvents();
}
