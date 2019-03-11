#ifndef __ARMORY_SPLASH_SCREEN_H__
#define __ARMORY_SPLASH_SCREEN_H__

#include <QSplashScreen>
#include <QProgressBar>
#include <QPixmap>
#include <QLabel>

class BSTerminalSplashScreen : public QSplashScreen
{
Q_OBJECT
public:
   explicit BSTerminalSplashScreen(const QPixmap& splash_image);
   ~BSTerminalSplashScreen() override;

   void SetProgress(int progressValue);
   int progress() const { return progress_->value(); }

public:
   void SetTipText(const QString& tip);

private:
   QProgressBar* progress_;
   QLabel* blockSettleLabel_;
};

#endif // __ARMORY_SPLASH_SCREEN_H__
