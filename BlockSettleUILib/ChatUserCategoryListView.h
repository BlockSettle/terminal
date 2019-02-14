#ifndef CHATUSERCATEGORYLISTVIEW_H
#define CHATUSERCATEGORYLISTVIEW_H

#include <QListView>
#include <QStyledItemDelegate>

class ChatUserCategoryListViewStyle : public QWidget
{
   Q_OBJECT

   Q_PROPERTY(QColor color_user_online READ colorUserOnline
              WRITE setColorUserOnline)
   Q_PROPERTY(QColor color_incoming_friend_request READ colorIncomingFriendRequest
              WRITE setColorIncomingFriendRequest)
   Q_PROPERTY(QColor color_user_default READ colorUserDefault
              WRITE setColorUserDefault)

public:
   inline explicit ChatUserCategoryListViewStyle(QWidget *parent)
      : QWidget(parent), _colorUserOnline(Qt::white), _colorIncomingFriendRequest(Qt::white),
        _colorUserDefault(Qt::white) 
   {
      setVisible(false);
   }

   QColor colorUserOnline() const { return _colorUserOnline; }
   void setColorUserOnline(const QColor &colorUserOnline) {
      _colorUserOnline = colorUserOnline;
   }

   QColor colorIncomingFriendRequest() const { return _colorIncomingFriendRequest; }
   void setColorIncomingFriendRequest(const QColor &colorIncomingFriendRequest) {
      _colorIncomingFriendRequest = colorIncomingFriendRequest;
   }

   QColor colorUserDefault() const { return _colorUserDefault; }
   void setColorUserDefault(const QColor &colorUserDefault) {
      _colorUserDefault = colorUserDefault;
   }

private:
   QColor _colorUserOnline;
   QColor _colorIncomingFriendRequest;
   QColor _colorUserDefault;
};

class ChatUserCategoryListViewDelegate : public QStyledItemDelegate
{
   Q_OBJECT

public:
   ChatUserCategoryListViewDelegate(const ChatUserCategoryListViewStyle &style,
                                    QObject *parent = nullptr);

   void paint(QPainter *painter,
              const QStyleOptionViewItem &option,
              const QModelIndex &index) const override;

private:
   const ChatUserCategoryListViewStyle &_internalStyle;
};

class ChatUserCategoryListView : public QListView
{
   Q_OBJECT
public:
   ChatUserCategoryListView(QWidget *parent = nullptr);

   using QListView::contentsSize;

private:
   ChatUserCategoryListViewStyle _internalStyle;
};

#endif // CHATUSERCATEGORYLISTVIEW_H
