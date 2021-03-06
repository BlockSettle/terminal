/*

***********************************************************************************
* Copyright (C) 2019 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef CHAT_SEARCH_LINE_EDIT_H
#define CHAT_SEARCH_LINE_EDIT_H

#include <memory>
#include <QLineEdit>

class ChatSearchLineEdit :
   public QLineEdit
{
   Q_OBJECT
public:
   ChatSearchLineEdit(QWidget *parent = nullptr);
   ~ChatSearchLineEdit() override;
   //void setActionsHandler(std::shared_ptr<ChatSearchActionsHandler> handler);
   void setResetOnNextInput(bool value);
private:
   void onTextChanged(const QString& text);
private:
   //std::shared_ptr<ChatSearchActionsHandler> handler_;
   bool resetOnNextInput_;

   // QWidget interface
protected:
   void keyPressEvent(QKeyEvent *event) override;

signals:
   void keyDownPressed();
   void keyEnterPressed();
   void keyEscapePressed();
};

#endif //CHAT_SEARCH_LINE_EDIT_H
