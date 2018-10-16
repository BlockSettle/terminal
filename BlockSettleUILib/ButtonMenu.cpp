#include "ButtonMenu.h"

#include <QPoint>
#include <QRect>

ButtonMenu::ButtonMenu(QPushButton* button)
 : QMenu(button)
 , parentButton_(button)
{}

void ButtonMenu::showEvent(QShowEvent* event)
{
   const auto currentPosition = pos();
   const auto buttonGeometry = parentButton_->geometry();

#if defined (Q_OS_WIN)
   const auto buttonPosition = parentButton_->pos();
   move(buttonPosition.x() + buttonGeometry.width() - geometry().width(), currentPosition.y());
#else
   move(currentPosition.x() + buttonGeometry.width() - geometry().width(), currentPosition.y());
#endif
}