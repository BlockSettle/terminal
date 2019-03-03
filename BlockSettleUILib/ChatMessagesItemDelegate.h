#ifndef __CHAT_MESSAGES_ITEM_DELEGATE_H__
#define __CHAT_MESSAGES_ITEM_DELEGATE_H__

#include <QStyledItemDelegate>

class ChatMessagesItemDelegate : public QStyledItemDelegate {
    Q_OBJECT

public:
    explicit ChatMessagesItemDelegate(QObject *parent = 0);

    QString anchorAt(QString html, const QPoint &point) const;

protected:
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const;
    // QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const;
};

#endif
