#include "ChatSearchPopup.h"
#include "ui_ChatSearchPopup.h"

#include <QLabel>
#include <QMenu>
#include <QAction>

ChatSearchPopup::ChatSearchPopup(QWidget *parent) :
   QWidget(parent),
   ui_(new Ui::ChatSearchPopup)
{
   setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
   ui_->setupUi(this);
   ui_->chatSearchPopupLabel->setContextMenuPolicy(Qt::CustomContextMenu);
   connect(ui_->chatSearchPopupLabel, &QLabel::customContextMenuRequested, this, &ChatSearchPopup::showMenu);
   searchPopupMenu_ = new QMenu(this);
   QAction *addUserToContactsAction = searchPopupMenu_->addAction(QObject::tr("Add to contacts"));
   addUserToContactsAction->setStatusTip(QObject::tr("Click to add user to contact list"));
   connect(addUserToContactsAction, &QAction::triggered,
      [this](bool) { emit sendFriendRequest(ui_->chatSearchPopupLabel->text()); }
   );
}

ChatSearchPopup::~ChatSearchPopup()
{
   searchPopupMenu_->deleteLater();
   delete ui_;
}

void ChatSearchPopup::setText(const QString &text)
{
   ui_->chatSearchPopupLabel->setText(text);
}

void ChatSearchPopup::showMenu(const QPoint &pos)
{
   searchPopupMenu_->exec(mapToGlobal(pos));
}

void ChatSearchPopup::setCustomPosition(const QWidget *widget, const int &moveX, const int &moveY)
{
   QPoint newPos = widget->mapToGlobal(widget->rect().bottomLeft());
   newPos.setX(newPos.x() + moveX);
   newPos.setY(newPos.y() + moveY);
   this->move(newPos);
}
