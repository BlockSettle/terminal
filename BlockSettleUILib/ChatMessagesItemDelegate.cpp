#include <QPainter>
#include <QTextDocument>
#include <QAbstractTextDocumentLayout>
#include "ChatMessagesItemDelegate.h"

ChatMessagesItemDelegate::ChatMessagesItemDelegate(QObject *parent) :
    QStyledItemDelegate(parent)
{}

QString ChatMessagesItemDelegate::anchorAt(QString html, const QPoint &point) const {
    QTextDocument doc;
    doc.setHtml(html);

    auto textLayout = doc.documentLayout();
    Q_ASSERT(textLayout != 0);
    return textLayout->anchorAt(point);
}

void ChatMessagesItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const {
   QStyleOptionViewItem opt = option;
   opt.decorationAlignment = Qt::AlignLeft;
   QStyledItemDelegate::paint(painter, opt, index);
}