#include "ChatSearchLineEdit.h"
#include "ChatHandleInterfaces.h"
#include <QKeyEvent>
ChatSearchLineEdit::ChatSearchLineEdit(QWidget *parent)
   : QLineEdit(parent)
   , handler_(nullptr)
{
   connect(this, &QLineEdit::textChanged, this, &ChatSearchLineEdit::onTextChanged);
}

void ChatSearchLineEdit::setActionsHandler(std::shared_ptr<ChatSearchActionsHandler> handler)
{
   handler_ = handler;
}

void ChatSearchLineEdit::onTextChanged(const QString &text)
{
   if (text.isEmpty() && handler_){
      handler_->onActionResetSearch();
   }
}


ChatSearchLineEdit::~ChatSearchLineEdit() = default;

void ChatSearchLineEdit::keyPressEvent(QKeyEvent * e)
{
   //Qt::Key_Return - Main Enter key
   //Qt::Key_Enter  = Numpad Enter key
   if (e->key() == Qt::Key_Enter || e->key() == Qt::Key_Return) {
         qDebug("Return/Enter search press %d", e->key());
         if (handler_) {
            handler_->onActionSearchUsers(text().toStdString());
         }
      return e->ignore();
   }
   return QLineEdit::keyPressEvent(e);
}
