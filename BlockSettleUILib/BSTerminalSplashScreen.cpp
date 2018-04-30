#include "BSTerminalSplashScreen.h"

#include <QBitmap>

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

   progress_->setStyleSheet(QLatin1String("text-align: center; font-size: 8px; border-width: 0px;"));
   SetTipText(tr("Loading"));
}

BSTerminalSplashScreen::~BSTerminalSplashScreen()
{}

void BSTerminalSplashScreen::SetTipText(const QString& tip)
{
   progress_->setFormat(tip + QString::fromStdString(" %p%"));
}

void BSTerminalSplashScreen::SetProgress(int progressValue)
{
   progress_->setValue(progressValue);
}
