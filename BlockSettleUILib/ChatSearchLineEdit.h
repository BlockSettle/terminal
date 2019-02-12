#ifndef CHAT_SEARCH_LINE_EDIT_H
#define CHAT_SEARCH_LINE_EDIT_H

#include <QLineEdit>

class ChatSearchLineEdit :
   public QLineEdit
{
   Q_OBJECT
public:
   ChatSearchLineEdit(QWidget *parent = nullptr);
   virtual ~ChatSearchLineEdit();
};

#endif //CHAT_SEARCH_LINE_EDIT_H
