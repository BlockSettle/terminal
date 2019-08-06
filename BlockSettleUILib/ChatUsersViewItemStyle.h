#ifndef CHATUSERSVIEWITEMSTYLE_H
#define CHATUSERSVIEWITEMSTYLE_H

#include <QWidget>

// #UI_ChatParty : Names should be updated bellow as far as termins have been changed
class ChatUsersViewItemStyle : public QWidget
{
   Q_OBJECT
   Q_PROPERTY(QColor color_category_item READ colorCategoryItem WRITE setColorCategoryItem)
   Q_PROPERTY(QColor color_room READ colorRoom WRITE setColorRoom)
   Q_PROPERTY(QColor color_user_online READ colorUserOnline WRITE setColorUserOnline)
   Q_PROPERTY(QColor color_user_offline READ colorUserOffline WRITE setColorUserOffline)
   Q_PROPERTY(QColor color_contact_online READ colorContactOnline WRITE setColorContactOnline)
   Q_PROPERTY(QColor color_contact_offline READ colorContactOffline WRITE setColorContactOffline)
   Q_PROPERTY(QColor color_contact_incoming READ colorContactIncoming WRITE setColorContactIncoming)
   Q_PROPERTY(QColor color_contact_outgoing READ colorContactOutgoing WRITE setColorContactOutgoing)
   Q_PROPERTY(QColor color_contact_rejected READ colorContactRejected WRITE setColorContactRejected)
   Q_PROPERTY(QColor color_highlight_background READ colorHighlightBackground WRITE setColorHighlightBackground)
private:
   QColor colorCategoryItem_;
   QColor colorRoom_;
   QColor colorUserOnline_;
   QColor colorUserOffline_;
   QColor colorContactOnline_;
   QColor colorContactOffline_;
   QColor colorContactIncoming_;
   QColor colorContactOutgoing_;
   QColor colorContactRejected_;
   QColor colorHighlightBackground_;

public:
   explicit ChatUsersViewItemStyle(QWidget *parent = nullptr);

   QColor colorCategoryItem() const;
   QColor colorRoom() const;
   QColor colorUserOnline() const;
   QColor colorUserOffline() const;
   QColor colorContactOnline() const;
   QColor colorContactOffline() const;
   QColor colorContactIncoming() const;
   QColor colorContactOutgoing() const;
   QColor colorContactRejected() const;
   QColor colorHighlightBackground() const;

signals:

public slots:
   void setColorCategoryItem(QColor colorCategoryItem);
   void setColorRoom(QColor colorRoom);
   void setColorUserOnline(QColor colorUserOnline);
   void setColorUserOffline(QColor colorUserOffline);
   void setColorContactOnline(QColor colorContactOnline);
   void setColorContactOffline(QColor colorContactOffline);
   void setColorContactIncoming(QColor colorContactIncoming);
   void setColorContactOutgoing(QColor colorContactOutgoing);
   void setColorContactRejected(QColor colorContactRejected);
   void setColorHighlightBackground(QColor colorHighlightBackground);
};
#endif // CHATUSERSVIEWITEMSTYLE_H
