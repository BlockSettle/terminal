#include "DialogManager.h"

#include <QApplication>
#include <QDesktopWidget>


DialogManager::DialogManager(const QRect& mainWinowRect)
   : rowHeight_(0)
   , center_(mainWinowRect.x() + mainWinowRect.width() / 2, mainWinowRect.y())
{
   screenSize_ = QApplication::desktop()->screenGeometry();
   reset();
}

void DialogManager::reset()
{
   dialogOffset_ = center_;
}

void DialogManager::adjustDialogPosition(QDialog *dlg)
{
   if (!nbActiveDlgs_) {
      reset();
   }
   nbActiveDlgs_++;
   connect(dlg, &QDialog::finished, this, &DialogManager::onDialogFinished);
   dlg->setModal(false);

   if ( (dlg->width() > screenSize_.width()) || (dlg->height() > screenSize_.height())) {
      dlg->move(QPoint{0,0});
   } else {
      if (dialogOffset_ == center_) {
         dialogOffset_.setX(dialogOffset_.x() - dlg->width() / 2);
      }

      if (dialogOffset_.x() + dlg->width() > screenSize_.width()) {
         dialogOffset_.setX(0);

         dialogOffset_.setY(dialogOffset_.y() + rowHeight_);
         rowHeight_ = 0;
      }

      if (dialogOffset_.y() + dlg->height() > screenSize_.height()) {
         dialogOffset_ = QPoint{0,0};
         rowHeight_ = 0;
      }

      if (dlg->height() > rowHeight_) {
         rowHeight_ = dlg->height();
      }
   }

   dlg->move(dialogOffset_);
   dlg->show();

   dialogOffset_.setX( dialogOffset_.x() + dlg->width());
}

void DialogManager::onDialogFinished(int)
{
   if (!nbActiveDlgs_) {
      return;
   }
   nbActiveDlgs_--;
}
