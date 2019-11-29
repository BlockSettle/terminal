/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef SEARCHWIDGET_H
#define SEARCHWIDGET_H

#include <memory>

#include <QWidget>

#include "ChatProtocol/ChatClientService.h"

class QAbstractItemModel;
class ChatSearchActionsHandler;
class UserSearchModel;
class ChatClient;

namespace Ui {
   class SearchWidget;
}

namespace Chat {
   class UserData;
   class Data;
}

class SearchWidget : public QWidget
{
   Q_OBJECT

/* Properties Section Begin */
   Q_PROPERTY(bool lineEditEnabled
              READ isLineEditEnabled
              WRITE onSetLineEditEnabled
              STORED false)
   Q_PROPERTY(bool listVisible
              READ isListVisible
              WRITE onSetListVisible
              STORED false)
   Q_PROPERTY(QString searchText
              READ searchText
              WRITE onSetSearchText
              NOTIFY searchTextChanged
              USER true
              STORED false)

public:
   bool isLineEditEnabled() const;
   bool isListVisible() const;
   QString searchText() const;

public slots:
   void onSetLineEditEnabled(bool value);
   void onSetListVisible(bool value);
   void onSetSearchText(QString value);

signals:
   void searchTextChanged(QString searchText);
/* Properties Section End */

public:
   explicit SearchWidget(QWidget *parent = nullptr);
   ~SearchWidget() override;

   bool eventFilter(QObject *watched, QEvent *event) override;

   void init(const Chat::ChatClientServicePtr& chatClientServicePtr);

public slots:
   void onClearLineEdit();
   void onStartListAutoHide();
   void onSearchUserReply(const Chat::SearchUserReplyList& userHashList, const std::string& searchId);

private slots:
   void onResetTreeView();
   void onShowContextMenu(const QPoint &pos);
   void onFocusResults();
   void onCloseResult();
   void onItemClicked(const QModelIndex &index);
   void onLeaveSearchResults();
   void onLeaveAndCloseSearchResults();
   void onInputTextChanged(const QString &text);
   void onSearchUserTextEdited();

signals:
   void contactFriendRequest(const QString &userID);
   void showUserRoom(const QString &userID);

private:
   QScopedPointer<Ui::SearchWidget> ui_;
   QScopedPointer<QTimer>           listVisibleTimer_;
   QScopedPointer<UserSearchModel>  userSearchModel_;
   Chat::ChatClientServicePtr       chatClientServicePtr_;
   std::string                      lastSearchId_;
};

#endif // SEARCHWIDGET_H
