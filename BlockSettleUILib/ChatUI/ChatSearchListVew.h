/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef CHATSEARCHLISTVEW_H
#define CHATSEARCHLISTVEW_H

#include <QTreeView>

class ChatSearchListVew : public QTreeView
{
   Q_OBJECT
public:
   explicit ChatSearchListVew(QWidget *parent = nullptr);

protected:
   void keyPressEvent(QKeyEvent *event) override;

signals:
   void leaveRequired();
   void leaveWithCloseRequired();
};

#endif // CHATSEARCHLISTVEW_H
