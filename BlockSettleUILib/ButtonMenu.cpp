#include "ButtonMenu.h"

#include <QPoint>
#include <QRect>

ButtonMenu::ButtonMenu(QPushButton* button)
 : QMenu(button)
 , parentButton_(button)
{}

void ButtonMenu::showEvent(QShowEvent* event)
{
   QPoint currentPosition = pos();
   QRect buttonGeometry = parentButton_->geometry();

   move(currentPosition.x() + buttonGeometry.width() - geometry().width(), currentPosition.y());
}