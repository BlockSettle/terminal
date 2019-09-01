#include "BSChatInput.h"

#include <QKeyEvent>

BSChatInput::BSChatInput(QWidget *parent)
   : QTextBrowser(parent)
{

}
BSChatInput::BSChatInput(const QString &text, QWidget *parent)
   : QTextBrowser(parent)
{
   setText(text);
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
   }
   else if (e->key() == Qt::Key_V && e->modifiers().testFlag(Qt::ControlModifier)) {
      QTextBrowser::keyPressEvent(e);
      auto cursor = textCursor();
      cursor.setCharFormat({});
      setTextCursor(cursor);
      return;
   }

   return QTextBrowser::keyPressEvent(e);
}
