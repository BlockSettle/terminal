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
      : QWidget(parent), colorUserOnline_(Qt::white), colorIncomingFriendRequest_(Qt::white),
        colorUserDefault_(Qt::white) 
   {
      setVisible(false);
   }

   QColor colorUserOnline() const { return colorUserOnline_; }
   void setColorUserOnline(const QColor &colorUserOnline) {
      colorUserOnline_ = colorUserOnline;
   }

   QColor colorIncomingFriendRequest() const { return colorIncomingFriendRequest_; }
   void setColorIncomingFriendRequest(const QColor &colorIncomingFriendRequest) {
      colorIncomingFriendRequest_ = colorIncomingFriendRequest;
   }

   QColor colorUserDefault() const { return colorUserDefault_; }
   void setColorUserDefault(const QColor &colorUserDefault) {
      colorUserDefault_ = colorUserDefault;
   }

private:
   QColor colorUserOnline_;
   QColor colorIncomingFriendRequest_;
   QColor colorUserDefault_;
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
   const ChatUserCategoryListViewStyle &internalStyle_;
};

class ChatUserCategoryListView : public QListView
{
   Q_OBJECT
public:
   ChatUserCategoryListView(QWidget *parent = nullptr);

   using QListView::contentsSize;

private:
   ChatUserCategoryListViewStyle internalStyle_;
};

#endif // CHATUSERCATEGORYLISTVIEW_H
