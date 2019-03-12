#include "ChatRoomsCategoryListView.h"

ChatRoomsCategoryListView::ChatRoomsCategoryListView(QWidget* parent)
: QListView (parent), internalStyle_(this)
{
   setItemDelegate(new ChatRoomsCategoryListViewDelegate(internalStyle_, this));
}

ChatRoomsCategoryListViewDelegate::ChatRoomsCategoryListViewDelegate(const ChatRoomsCategoryListViewStyle& style, QObject* parent)
: QStyledItemDelegate (parent), internalStyle_(style)
{
    
}

void ChatRoomsCategoryListViewDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QStyleOptionViewItem itemOption(option);
    itemOption.palette.setColor(QPalette::Text, internalStyle_.colorRoom());
    itemOption.palette.setColor(QPalette::HighlightedText, internalStyle_.colorRoom());
    QStyledItemDelegate::paint(painter, itemOption, index);
    return QStyledItemDelegate::paint(painter, itemOption, index);
}
