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
   void setCustomPosition(const QWidget *widget, const int &moveX, const int &moveY);

signals:
   void sendFriendRequest(const QString &userID);

private slots:
   void showMenu(const QPoint &pos);

private:
   Ui::ChatSearchPopup *ui_;
   QMenu *searchPopupMenu_;
   QString userID_;
};

#endif // CHATSEARCHPOPUP_H
