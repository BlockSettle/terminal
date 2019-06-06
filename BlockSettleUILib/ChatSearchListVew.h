#ifndef CHATSEARCHLISTVEW_H
#define CHATSEARCHLISTVEW_H

#include <QTreeView>

class ChatSearchListVew : public QTreeView
{
   Q_OBJECT
public:
   explicit ChatSearchListVew(QWidget *parent = nullptr);
};

#endif // CHATSEARCHLISTVEW_H
