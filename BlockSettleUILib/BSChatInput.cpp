#include "BSChatInput.h"

#include <QKeyEvent>

BSChatInput::BSChatInput(QWidget *parent)
   : QTextEdit(parent)
{

}
BSChatInput::BSChatInput(const QString &text, QWidget *parent)
   : QTextEdit(text, parent)
{

}

BSChatInput::~BSChatInput() = default;

void BSChatInput::keyPressEvent(QKeyEvent * e)
{
   //Qt::Key_Return - Main Enter key
   //Qt::Key_Enter  = Numpad Enter key
   if (e->key() == Qt::Key_Enter || e->key() == Qt::Key_Return) {
      if (e->modifiers().testFlag(Qt::ShiftModifier)) {
         this->insertPlainText(QStringLiteral("\n"));

      } else {
         emit sendMessage();
      }
      return e->ignore();
   } else if(e->key() == Qt::Key_C && e->modifiers().testFlag(Qt::ControlModifier)) {
      // If there no selection than could be that we going to copy text from other element
      // which cannot have focus.
      if (!textCursor().hasSelection()) {
         e->setAccepted(false);
         return;
      }
   }

   return QTextEdit::keyPressEvent(e);
}
