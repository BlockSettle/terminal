/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "ChatSearchPopup.h"
#include "ui_ChatSearchPopup.h"

#include <QLabel>
#include <QMenu>
#include <QAction>

ChatSearchPopup::ChatSearchPopup(QWidget *parent) :
   QWidget(parent),
   userID_(),
   ui_(new Ui::ChatSearchPopup)
{
   ui_->setupUi(this);

   ui_->chatSearchPopupLabel->setContextMenuPolicy(Qt::CustomContextMenu);
   connect(ui_->chatSearchPopupLabel, &QLabel::customContextMenuRequested, this, &ChatSearchPopup::onShowMenu);

   searchPopupMenu_ = new QMenu(this);   
   userContactAction_ = searchPopupMenu_->addAction(QString());
}

ChatSearchPopup::~ChatSearchPopup()
{
   searchPopupMenu_->deleteLater();
   delete ui_;
}

void ChatSearchPopup::setUserID(const QString &userID)
{
   userID_ = userID;
   
   if (userID_.isEmpty()) {
      ui_->chatSearchPopupLabel->setText(tr("User not found"));
   }
   else {
      ui_->chatSearchPopupLabel->setText(userID_);
   }
}

void ChatSearchPopup::setUserIsInContacts(const bool &isInContacts)
{
   isInContacts_ = isInContacts;
}

void ChatSearchPopup::onShowMenu(const QPoint &pos)
{
   if (!userID_.isEmpty()) {

      userContactAction_->disconnect();

      if (!isInContacts_) {
         userContactAction_->setText(tr("Add to contacts"));
         userContactAction_->setStatusTip(QObject::tr("Click to add user to contact list"));
         connect(userContactAction_, &QAction::triggered,
            [this](bool) { emit sendFriendRequest(ui_->chatSearchPopupLabel->text()); }
         );
      }
      else {
         userContactAction_->setText(tr("Remove from contacts"));
         userContactAction_->setStatusTip(QObject::tr("Click to remove user from contact list"));
         connect(userContactAction_, &QAction::triggered,
            [this](bool) { emit removeFriendRequest(ui_->chatSearchPopupLabel->text()); }
         );
      }

      searchPopupMenu_->exec(mapToGlobal(pos));
   }
}

void ChatSearchPopup::setCustomPosition(const QWidget *widget, const int &moveX, const int &moveY)
{
   QPoint newPos = widget->mapToGlobal(widget->rect().bottomLeft());
   newPos.setX(newPos.x() + moveX);
   newPos.setY(newPos.y() + moveY);
   this->move(newPos);
}
