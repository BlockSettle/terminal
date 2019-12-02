/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
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
