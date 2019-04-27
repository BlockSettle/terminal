#ifndef CHAT_SEARCH_LINE_EDIT_H
#define CHAT_SEARCH_LINE_EDIT_H

#include <memory>
#include <QLineEdit>

class ChatSearchActionsHandler;
class ChatSearchLineEdit :
   public QLineEdit
{
   Q_OBJECT
public:
   ChatSearchLineEdit(QWidget *parent = nullptr);
   virtual ~ChatSearchLineEdit() override;
   void setActionsHandler(std::shared_ptr<ChatSearchActionsHandler> handler);
private:
   void onTextChanged(const QString& text);
private:
   std::shared_ptr<ChatSearchActionsHandler> handler_;

   // QWidget interface
protected:
   void keyPressEvent(QKeyEvent *event) override;
};



#endif //CHAT_SEARCH_LINE_EDIT_H
