#ifndef CHATSEARCHLISTVEW_H
#define CHATSEARCHLISTVEW_H

#include <QTreeView>

class ChatSearchListVew : public QTreeView
{
   Q_OBJECT
public:
   explicit ChatSearchListVew(QWidget *parent = nullptr);

protected:
   void keyPressEvent(QKeyEvent *event) override;

signals:
   void leaveRequired();
   void leaveWithCloseRequired();
};

#endif // CHATSEARCHLISTVEW_H
