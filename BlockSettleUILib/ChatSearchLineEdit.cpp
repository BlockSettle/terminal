#include "ChatSearchLineEdit.h"

#include <QtDebug>

ChatSearchLineEdit::ChatSearchLineEdit(QWidget *parent) : QLineEdit(parent)
{
	connect(this, &ChatSearchLineEdit::editingFinished, this, &ChatSearchLineEdit::onEditingFinished);
}


ChatSearchLineEdit::~ChatSearchLineEdit() = default;

void ChatSearchLineEdit::onEditingFinished()
{
	qDebug() << "Editing finished";
	qWarning() << "TEST3 !!!!!!";
}
