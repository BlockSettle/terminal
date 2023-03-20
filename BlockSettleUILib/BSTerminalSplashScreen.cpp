/*

***********************************************************************************
* Copyright (C) 2018 - 2020, BlockSettle AB
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
   progress_->setMinimumWidth(this->width() - 10);
   progress_->setMaximumHeight(8);
   progress_->move(5, 289);

   //blockSettleLabel_ = new QLabel(this);
   //blockSettleLabel_->setText(QLatin1String("BLOCKSETTLE TERMINAL"));
   //blockSettleLabel_->move(30, 140);
   //blockSettleLabel_->setStyleSheet(QLatin1String("font-size: 18px; color: white"));

   progress_->setStyleSheet(QLatin1String("QProgressBar::chunk{background-color:#45A6FF}; background:#191E2A; text-align: center; font-size: 8px; border-width: 1px; border-color: #3C435A"));
   //SetTipText(tr("Loading"));
   progress_->setFormat(QString::fromStdString(""));
}

BSTerminalSplashScreen::~BSTerminalSplashScreen() = default;

void BSTerminalSplashScreen::SetTipText(const QString& tip)
{
   //progress_->setFormat(tip + QString::fromStdString(" %p%"));
//   QApplication::processEvents();
}

void BSTerminalSplashScreen::SetProgress(int progressValue)
{
   progress_->setValue(progressValue);
//   QApplication::processEvents();
}
