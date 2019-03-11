#include "ChatRoomsCategoryListView.h"

ChatRoomsCategoryListView::ChatRoomsCategoryListView(QWidget* parent)
: QListView (parent), _internalStyle(this)
{
   setItemDelegate(new ChatRoomsCategoryListViewDelegate(_internalStyle, this));
}

ChatRoomsCategoryListViewDelegate::ChatRoomsCategoryListViewDelegate(const ChatRoomsCategoryListViewStyle& style, QObject* parent)
: QStyledItemDelegate (parent), _internalStyle(style)
{
    
}

void ChatRoomsCategoryListViewDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QStyleOptionViewItem itemOption(option);
    itemOption.palette.setColor(QPalette::Text, _internalStyle.colorRoom());
    itemOption.palette.setColor(QPalette::HighlightedText, _internalStyle.colorRoom());
    QStyledItemDelegate::paint(painter, itemOption, index);
    return QStyledItemDelegate::paint(painter, itemOption, index);
}
