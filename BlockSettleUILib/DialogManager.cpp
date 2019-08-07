#include "DialogManager.h"

#include <QApplication>
#include <QDesktopWidget>
#include <QWidget>
#include <cmath>
#include "QLayout"

DialogManager::DialogManager(const QWidget *mainWindow)
   : mainWindow_(mainWindow)
{
}

void DialogManager::adjustDialogPosition(QDialog *dlg)
{
   if (!prepare(dlg))
      return;

   connect(dlg, &QDialog::finished, this, &DialogManager::onDialogFinished);
   dlg->setModal(false);
   dlg->layout()->update();
   dlg->layout()->activate();

   const QPoint center = getGeometry(mainWindow_).center();

   const QRect inputGeometry = getGeometry(dlg);
   const QRect screenSize = getGeometry(QApplication::desktop());
   const QRect available = QApplication::desktop()->availableGeometry();
   // For windows those value could be negative
   // For Ubuntu & OSX they should be zero
   const int deltaX = std::abs(screenSize.x());
   const int deltaY = std::abs(screenSize.y());

   // Determinate dialog top left position
   QPoint dialogTopLeft;
   dialogTopLeft.setX(std::max(screenSize.x(), center.x() - (inputGeometry.width() / 2)));
   dialogTopLeft.setY(std::max(screenSize.y(), center.y() - (inputGeometry.height() / 2)));

   auto findScreenNumder = [](int value, int measure) -> int {
      int screenNumber = 0;
      for (; value >= measure; value -= measure) {
         ++screenNumber;
      }
      return screenNumber;
   };

   // Determine screen position
   const int horScreenNumber = findScreenNumder(center.x() + deltaX, available.width());
   const int vertScreenNumber = findScreenNumder(center.y() + deltaY, available.height());

   const QPoint screenTopLeft = { horScreenNumber * available.width() - deltaX,
                           vertScreenNumber * available.height() - deltaY };
   const QPoint screenBottomRight = { screenTopLeft.x() + available.width(),
                              screenTopLeft.y() + available.height() };


   auto adjustX = [&](QDialog *dlg) {
      if (dialogTopLeft.x() < screenTopLeft.x()) {
         dialogTopLeft.setX(screenTopLeft.x());
      }
      else if (dialogTopLeft.x() + getGeometry(dlg).width() > screenBottomRight.x()) {
         const int diffX = dialogTopLeft.x() + getGeometry(dlg).width() - screenBottomRight.x();
         dialogTopLeft.setX(std::max(dialogTopLeft.x() - diffX, screenTopLeft.x()));
      }
   };

   auto adjustY = [&](QDialog *dlg) {
      if (dialogTopLeft.y() < screenTopLeft.y()) {
         dialogTopLeft.setY(screenTopLeft.y());
      }
      else if (dialogTopLeft.y() + getGeometry(dlg).height() > screenBottomRight.y()) {
         const int diffY = dialogTopLeft.y() + getGeometry(dlg).height() - screenBottomRight.y();
         dialogTopLeft.setY(std::max(dialogTopLeft.y() - diffY, screenTopLeft.y()));
      }
   };


   if (QApplication::desktop()->screenCount() == 1) {
      if ((inputGeometry.width() > available.width()) || (inputGeometry.height() > available.height())) {
         dialogTopLeft = { 0, 0 };
      }
   }
   else {
      adjustX(dlg);
      adjustY(dlg);
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

      const QRect otherGeometry = getGeometry(other.data());
      QPoint otherP = otherGeometry.topLeft();
      QPoint delta = dialogTopLeft - otherP;

      // If there less then 5 pixels difference
      // update position.
      if (delta.manhattanLength() <= 5) {
         dialogTopLeft.setX(dialogTopLeft.x() + otherGeometry.width());
         adjustX(other);

         // Check again - if true we in the same position
         delta = dialogTopLeft - otherP;
         if (delta.manhattanLength() <= 5) {
            int const startX = center.x() - (inputGeometry.width() / 2);
            if (dialogTopLeft.x() == startX) {
               dialogTopLeft.setX(dialogTopLeft.x() + otherGeometry.width());
            } else {
               dialogTopLeft.setX(startX);
            }
            dialogTopLeft.setY(otherP.y() + otherGeometry.height());
            adjustX(other);
            adjustY(other);
         }
      }
   }

   dlg->move(dialogTopLeft);
   activeDlgs_.push_back(QPointer<QDialog>(dlg));

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

bool DialogManager::prepare(QDialog* dlg)
{
#ifndef QT_NO_DEBUG
   Q_ASSERT(dlg);
   Q_ASSERT(mainWindow_);
#endif
   if (!dlg) {
      return false;
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
   std::sort(activeDlgs_.begin(), activeDlgs_.end(), [this](const auto left, const auto right) {
#ifndef QT_NO_DEBUG
      // We expected all dialogs are alive
      Q_ASSERT(left);
      Q_ASSERT(right);
#endif
      const QPoint leftP = getGeometry(left.data()).topLeft();
      const QPoint rightP = getGeometry(right.data()).topLeft();

      if (leftP.y() != rightP.y()) {
         return leftP.y() < rightP.y();
      }

      return leftP.x() < rightP.x();
   });

   return true;
}

const QRect DialogManager::getGeometry(const QWidget* widget) const
{
   // We use different rect for different system.
   // On windows frameGeometry != geometry
#ifdef Q_OS_WIN
   return widget->frameGeometry();
#else
   return widget->geometry();
#endif
}
