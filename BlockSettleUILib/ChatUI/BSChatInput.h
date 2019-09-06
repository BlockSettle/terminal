#ifndef BSCHATINPUT_H
#define BSCHATINPUT_H

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

#endif // BSCHATINPUT_H
