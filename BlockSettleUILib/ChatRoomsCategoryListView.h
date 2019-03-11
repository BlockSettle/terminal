#ifndef CHATROOMSCATEGORYLISTVIEW_H
#define CHATROOMSCATEGORYLISTVIEW_H

#include <QListView>
#include <QStyledItemDelegate>

class ChatRoomsCategoryListViewStyle : public QWidget
{
    Q_OBJECT
    
    Q_PROPERTY(QColor color_room READ colorRoom
               WRITE setColorRoom)
    
public:
    ChatRoomsCategoryListViewStyle(QWidget* parent)
    : QWidget(parent), _color_room(Qt::white)
    {
        setVisible(false);
    }
    
    QColor colorRoom() const
    {
        return _color_room;
    }
    
public slots:
    void setColorRoom(QColor color_room)
    {
        _color_room = color_room;
    }
    
private:
    QColor _color_room;
};

class ChatRoomsCategoryListViewDelegate : public QStyledItemDelegate
{
   Q_OBJECT

public:
   ChatRoomsCategoryListViewDelegate(const ChatRoomsCategoryListViewStyle &style,
                                    QObject *parent = nullptr);

   void paint(QPainter *painter,
              const QStyleOptionViewItem &option,
              const QModelIndex &index) const override;

private:
   const ChatRoomsCategoryListViewStyle &_internalStyle;
};

class ChatRoomsCategoryListView : public QListView
{
    Q_OBJECT
public:
   ChatRoomsCategoryListView(QWidget* parent = nullptr);
   using QListView::contentsSize;
private:
   ChatRoomsCategoryListViewStyle _internalStyle;
};

#endif // CHATROOMSCATEGORYLISTVIEW_H
