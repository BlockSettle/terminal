#ifndef CHATSEARCHPOPUP_H
#define CHATSEARCHPOPUP_H

#include <QWidget>

namespace Ui {
class ChatSearchPopup;
}

class QMenu;

class ChatSearchPopup : public QWidget
{
   Q_OBJECT

public:
   explicit ChatSearchPopup(QWidget *parent = nullptr);
   ~ChatSearchPopup();

   void setUserID(const QString &userID);
   void setUserIsInContacts(const bool &isInContacts);
   void setCustomPosition(const QWidget *widget, const int &moveX, const int &moveY);

signals:
   void sendFriendRequest(const QString &userID);
   void removeFriendRequest(const QString &userID);

private slots:
   void onShowMenu(const QPoint &pos);

private:
   Ui::ChatSearchPopup *ui_;
   QMenu *searchPopupMenu_;
   QString userID_;
   bool isInContacts_;
   QAction *userContactAction_;
};

#endif // CHATSEARCHPOPUP_H
