#include "ButtonMenu.h"

#include <QPoint>
#include <QRect>
#include <QApplication>

ButtonMenu::ButtonMenu(QPushButton* button)
 : QMenu(button)
 , parentButton_(button)
{}

void ButtonMenu::showEvent(QShowEvent* event) {
   // convert button position to global coordinates
   const auto buttonPosition = QApplication::activeWindow()->mapToGlobal(parentButton_->pos());
   // always align the menu on the right side of the button
   move(buttonPosition.x() + parentButton_->width() - width(), pos().y());
}