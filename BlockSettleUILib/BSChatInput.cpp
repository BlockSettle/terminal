#include "BSChatInput.h"
//#include "ui_BSChatInput.h"

BSChatInput::BSChatInput(QWidget *parent)
	: QTextEdit(parent)
{

}
BSChatInput::BSChatInput(const QString &text, QWidget *parent)
	: QTextEdit(text, parent)
{

}

BSChatInput::~BSChatInput() = default;
