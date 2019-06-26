#ifndef CHATSEARCHLISTVIEWITEMSTYLE_H
#define CHATSEARCHLISTVIEWITEMSTYLE_H

#include <QWidget>

class ChatSearchListViewItemStyle : public QWidget
{
   Q_OBJECT
   Q_PROPERTY(QColor color_contact_unknown MEMBER colorContactUnknown_)
   Q_PROPERTY(QColor color_contact_accepted MEMBER colorContactAccepted_)
   Q_PROPERTY(QColor color_contact_incoming MEMBER colorContactIncoming_)
   Q_PROPERTY(QColor color_contact_outgoing MEMBER colorContactOutgoing_)
   Q_PROPERTY(QColor color_contact_rejected MEMBER colorContactRejected_)

public:
   explicit ChatSearchListViewItemStyle(QWidget *parent = nullptr);

private:
   QColor colorContactUnknown_;
   QColor colorContactAccepted_;
   QColor colorContactIncoming_;
   QColor colorContactOutgoing_;
   QColor colorContactRejected_;

};

#endif // CHATSEARCHLISTVIEWITEMSTYLE_H
