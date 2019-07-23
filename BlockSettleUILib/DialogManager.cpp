#include "DialogManager.h"

#include <QApplication>
#include <QDesktopWidget>
#include <QWidget>
#include <cmath>

DialogManager::DialogManager(const QWidget *mainWindow)
   : mainWindow_(mainWindow)
{
}

void DialogManager::adjustDialogPosition(QDialog *dlg)
{
#ifndef QT_NO_DEBUG
   Q_ASSERT(dlg);
   Q_ASSERT(mainWindow_);
#endif
   if (!dlg) {
      return;
   }

   // Sanity check
   auto startDlg = std::remove_if(activeDlgs_.begin(), activeDlgs_.end(), [](const auto dlg) {
#ifndef QT_NO_DEBUG
      // We expected all dialogs are alive
      Q_ASSERT(dlg);
#endif
      return dlg.isNull();
   });
   activeDlgs_.erase(startDlg, activeDlgs_.end());

   // Sort dialog by position, so we could determinate correct point to insert current one
   std::sort(activeDlgs_.begin(), activeDlgs_.end(), [](const auto left, const auto right) {
#ifndef QT_NO_DEBUG
      // We expected all dialogs are alive
      Q_ASSERT(left);
      Q_ASSERT(right);
#endif
      const QPoint leftP = { left.data()->x(), left.data()->y() };
      const QPoint rightP = { right.data()->x(), right.data()->y() };
      return leftP.manhattanLength() < rightP.manhattanLength();
   });
   connect(dlg, &QDialog::finished, this, &DialogManager::onDialogFinished);
   dlg->setModal(false);

   const QPoint center = mainWindow_->geometry().center();
   // Determinate dialog top left position
   QPoint dialogTopLeft;
   dialogTopLeft.setX(std::max(0, center.x() - (dlg->width() / 2)));
   dialogTopLeft.setY(std::max(0, center.y() - (dlg->height() / 2)));

   const QRect& screenSize = QApplication::desktop()->geometry();
   if (QApplication::desktop()->screenCount() == 1) {
      if ((dlg->width() > screenSize.width()) || (dlg->height() > screenSize.height())) {
         dialogTopLeft = { 0, 0};
      }
   } else {
      auto findScreenNumder = [](int value, int measure) -> int {
         int screenNumber = 0;
         for (; value >= measure; value -= measure) {
            ++screenNumber;
         }
         return screenNumber;
      };

      // Determine screen position
      const int horScreenNumber = findScreenNumder(center.x(), screenSize.width());
      const int vertScreenNumber = findScreenNumder(center.y(), screenSize.height());

      const QPoint screenTopLeft = { horScreenNumber * screenSize.width(),
                                     vertScreenNumber * screenSize.height()};
      const QPoint screenBottomRight = { (horScreenNumber + 1) * screenSize.width(),
                                         (vertScreenNumber + 1) * screenSize.height()};

      // Adjust X
      if (dialogTopLeft.x() < screenTopLeft.x()) {
         dialogTopLeft.setX(screenTopLeft.x());
      } else if (dialogTopLeft.x() + dlg->width() > screenBottomRight.x()) {
         const int diffX = dialogTopLeft.x() + dlg->width() - screenBottomRight.x();
         dialogTopLeft.setX(std::max(dialogTopLeft.x() - diffX, screenTopLeft.x()));
      }

      // Adjust Y
      if (dialogTopLeft.y() < screenTopLeft.y()) {
         dialogTopLeft.setY(screenTopLeft.y());
      } else if (dialogTopLeft.y() + dlg->height() > screenBottomRight.y()) {
         const int diffY = dialogTopLeft.y() + dlg->height() - screenBottomRight.y();
         dialogTopLeft.setY(std::max(dialogTopLeft.y() - diffY, screenTopLeft.y()));
      }
   }

   // Make sure that we do not overlap other dialog
   for (int i = 0; i < activeDlgs_.size(); ++i) {
      const auto &other = activeDlgs_[i];

#ifndef QT_NO_DEBUG
      Q_ASSERT(!other.isNull());
#endif
      if (other.isNull()) {
         continue;
      }

      const QPoint otherP = other.data()->geometry().topLeft();
      const QPoint delta = dialogTopLeft - otherP;
      // If there less then 5 pixels difference
      // update position.
      if (delta.manhattanLength() <= 5) {
         dialogTopLeft.setX(dialogTopLeft.x() + offset_);
         dialogTopLeft.setY(dialogTopLeft.y() + offset_);
      }
   }

   activeDlgs_.push_back(QPointer<QDialog>(dlg));
   dlg->move(dialogTopLeft);
   dlg->show();
}

void DialogManager::onDialogFinished()
{
   QDialog *dialog = qobject_cast<QDialog *>(sender());
#ifndef QT_NO_DEBUG
   Q_ASSERT(dialog);
#endif
   if (!dialog) {
      return;
   }

#ifndef QT_NO_DEBUG
   Q_ASSERT(activeDlgs_.size() > 0);
#endif
   for (int i = 0; i < activeDlgs_.size(); ++i) {
      if (activeDlgs_[i].data() == dialog) {
         activeDlgs_.removeAt(i);
         break;
      }
#ifndef QT_NO_DEBUG
      // Check that we didn't pass all elements
      Q_ASSERT(activeDlgs_.size() - 1 != i);
#endif
   }
}
