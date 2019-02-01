#include "chatsearchpopup.h"
#include "ui_chatsearchpopup.h"

#include <QLabel>
#include <QMenu>
#include <QAction>
#include <QtDebug>

ChatSearchPopup::ChatSearchPopup(QWidget *parent) :
   QWidget(parent),
   ui(new Ui::ChatSearchPopup)
{
   setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
   ui->setupUi(this);
   ui->chatSearchPopupLabel->setContextMenuPolicy(Qt::CustomContextMenu);
   connect(ui->chatSearchPopupLabel, &QLabel::customContextMenuRequested, this, &ChatSearchPopup::showMenu);
   _searchPopupMenu = new QMenu(this);
   QAction *addUserToContactsAction = _searchPopupMenu->addAction(QObject::tr("Add to contacts"));
   addUserToContactsAction->setStatusTip(QObject::tr("Click to add user to contact list"));
   connect(addUserToContactsAction, &QAction::triggered,
      [this](bool) { emit addUserToContacts(ui->chatSearchPopupLabel->text()); }
   );
}

ChatSearchPopup::~ChatSearchPopup()
{
   _searchPopupMenu->deleteLater();
   delete ui;
}

void ChatSearchPopup::setText(const QString &text)
{
   ui->chatSearchPopupLabel->setText(text);
}

void ChatSearchPopup::showMenu(const QPoint &pos)
{
   _searchPopupMenu->exec(mapToGlobal(pos));
}

void ChatSearchPopup::setCustomPosition(const QWidget *widget, const int &moveX, const int &moveY)
{
   QPoint newPos = widget->mapToGlobal(widget->rect().bottomLeft());
   newPos.setX(newPos.x() + moveX);
   newPos.setY(newPos.y() + moveY);
   this->move(newPos);
}
