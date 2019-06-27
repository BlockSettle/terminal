#include "ChatSearchLineEdit.h"
#include "ChatHandleInterfaces.h"
#include <QKeyEvent>
ChatSearchLineEdit::ChatSearchLineEdit(QWidget *parent)
   : QLineEdit(parent)
   , handler_(nullptr)
   , resetOnNextInput_(false)
{
   connect(this, &QLineEdit::textChanged, this, &ChatSearchLineEdit::onTextChanged);
}

void ChatSearchLineEdit::setActionsHandler(std::shared_ptr<ChatSearchActionsHandler> handler)
{
   handler_ = handler;
}

void ChatSearchLineEdit::setResetOnNextInput(bool value)
{
   resetOnNextInput_ = value;
}

void ChatSearchLineEdit::onTextChanged(const QString &text)
{
   if (text.isEmpty() && handler_) {
      handler_->onActionResetSearch();
   }
}


ChatSearchLineEdit::~ChatSearchLineEdit() = default;

void ChatSearchLineEdit::keyPressEvent(QKeyEvent * e)
{
   if (resetOnNextInput_) {
      clear();
      resetOnNextInput_ = false;
   }
   switch (e->key()) {
   case Qt::Key_Enter:     //Qt::Key_Enter   - Numpad Enter key
   case Qt::Key_Return:    //Qt::Key_Return  - Main Enter key
   {
      qDebug("Return/Enter search press %d", e->key());
      if (handler_) {
         handler_->onActionSearchUsers(text().toStdString());
      }
      return e->ignore();
   }
   case Qt::Key_Down:     //Qt::Key_Down     - For both standalone and Numpad arrow down keys
      emit keyDownPressed();
      break;
   default:
      break;
   }
   return QLineEdit::keyPressEvent(e);
}
