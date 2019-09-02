#ifndef __CHAT_INPUT_H__
#define __CHAT_INPUT_H__

#include <QTextBrowser>

class BSChatInput : public QTextBrowser {
   Q_OBJECT
public:
   BSChatInput(QWidget *parent = nullptr);
   BSChatInput(const QString &text, QWidget *parent = nullptr);
   ~BSChatInput() override;

signals:
   void sendMessage();

public:
   void keyPressEvent(QKeyEvent * e) override;
};

#endif // __CHAT_INPUT_H__
