#ifndef __CHAT_INPUT_H__
#define __CHAT_INPUT_H__

#include <QTextEdit>

/*namespace Ui
{
	class BSChatInput;
};*/

class BSChatInput : public QTextEdit {
	Q_OBJECT
public:
	BSChatInput(QWidget *parent = nullptr);
	BSChatInput(const QString &text, QWidget *parent = nullptr);
	~BSChatInput() override;

signals:
	void sendMessage();

protected:
	void keyPressEvent(QKeyEvent * e) override;
};

#endif // __CHAT_INPUT_H__
