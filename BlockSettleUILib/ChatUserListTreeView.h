#ifndef CHATUSERLISTTREEVIEW_H
#define CHATUSERLISTTREEVIEW_H

#include <QTreeView>
#include <QStyledItemDelegate>

#include "ChatUserListTreeViewModel.h"

class ChatUserListTreeViewStyle : public QWidget
{
   Q_OBJECT
    
   Q_PROPERTY(QColor color_room READ colorRoom
               WRITE setColorRoom)
   Q_PROPERTY(QColor color_user_online READ colorUserOnline
              WRITE setColorUserOnline)
   Q_PROPERTY(QColor color_incoming_friend_request READ colorIncomingFriendRequest
              WRITE setColorIncomingFriendRequest)
   Q_PROPERTY(QColor color_outgoing_friend_request READ colorOutgoingFriendRequest
              WRITE setColorOutgoinggFriendRequest)
   Q_PROPERTY(QColor color_user_default READ colorUserDefault
              WRITE setColorUserDefault)
    
public:
   ChatUserListTreeViewStyle(QWidget* parent)
   : QWidget(parent), colorRoom_(Qt::white), colorUserOnline_(Qt::white), colorIncomingFriendRequest_(Qt::white),
        colorUserDefault_(Qt::white) 
   {
      setVisible(false);
   }
    
   QColor colorRoom() const
   {
      return colorRoom_;
   }
   
   void setColorRoom(QColor colorRoom)
   {
      colorRoom_ = colorRoom;
   }

   QColor colorUserOnline() const 
   {
      return colorUserOnline_; 
   }

   void setColorUserOnline(const QColor &colorUserOnline) 
   {
      colorUserOnline_ = colorUserOnline;
   }

   QColor colorIncomingFriendRequest() const 
   {
      return colorIncomingFriendRequest_; 
   }

   void setColorIncomingFriendRequest(const QColor &colorIncomingFriendRequest)
   {
      colorIncomingFriendRequest_ = colorIncomingFriendRequest;
   }

   QColor colorUserDefault() const 
   { 
      return colorUserDefault_; 
   }

   void setColorUserDefault(const QColor &colorUserDefault) 
   {
      colorUserDefault_ = colorUserDefault;
   }

   QColor colorOutgoingFriendRequest() const
   {
      return colorOutgoingFriendRequest_;
   }

public slots:
   void setColorOutgoinggFriendRequest(QColor color_outgoing_friend_request)
   {
      colorOutgoingFriendRequest_ = color_outgoing_friend_request;
   }

private:
   QColor colorRoom_;
   QColor colorUserOnline_;
   QColor colorIncomingFriendRequest_;
   QColor colorUserDefault_;
   QColor colorOutgoingFriendRequest_;
};
class BSContextMenu;
class ChatUserListTreeViewDelegate : public QStyledItemDelegate
{
   Q_OBJECT

public:
   ChatUserListTreeViewDelegate(const ChatUserListTreeViewStyle &style,
                                    QObject *parent = nullptr);

   void paint(QPainter *painter,
              const QStyleOptionViewItem &option,
              const QModelIndex &index) const override;

private:
   const ChatUserListTreeViewStyle &internalStyle_;
};

class ChatUserListTreeView : public QTreeView
{
   Q_OBJECT

public:
   explicit ChatUserListTreeView(QWidget *parent = nullptr);

   ChatUserListTreeViewModel *chatUserListModel() const;

   void selectFirstRoom();
   void highlightUserItem(const QString& userId);

signals:
   void userClicked(const QString &userId);
   void roomClicked(const QString &roomId);
   void acceptFriendRequest(const QString &userId);
   void declineFriendRequest(const QString &userId);

public slots:
   void onChatUserDataChanged(const ChatUserDataPtr &chatUserDataPtr);
   void onChatUserDataListChanged(const ChatUserDataListPtr &chatUserDataListPtr);
   void onChatRoomDataChanged(const Chat::RoomDataPtr &roomsDataPtr);
   void onChatRoomDataListChanged(const Chat::RoomDataListPtr &roomsDataList);
   void onCustomContextMenu(const QPoint &);

private slots:
   void onUserListItemClicked(const QModelIndex &index);

private:
   ChatUserListTreeViewModel *chatUserListModel_;
   ChatUserListTreeViewStyle internalStyle_;
   BSContextMenu* contextMenu_;

};

#endif // CHATUSERLISTTREEVIEW_H
