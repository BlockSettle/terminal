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
    : QWidget(parent), colorRoom_(Qt::white)
    {
        setVisible(false);
    }
    
    QColor colorRoom() const
    {
        return colorRoom_;
    }
    
public slots:
    void setColorRoom(QColor colorRoom)
    {
        colorRoom_ = colorRoom;
    }
    
private:
    QColor colorRoom_;
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
   const ChatRoomsCategoryListViewStyle &internalStyle_;
};

class ChatRoomsCategoryListView : public QListView
{
    Q_OBJECT
public:
   ChatRoomsCategoryListView(QWidget* parent = nullptr);
   using QListView::contentsSize;
private:
   ChatRoomsCategoryListViewStyle internalStyle_;
};

#endif // CHATROOMSCATEGORYLISTVIEW_H
