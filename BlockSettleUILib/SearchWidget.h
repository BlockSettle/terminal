#ifndef SEARCHWIDGET_H
#define SEARCHWIDGET_H

#include <QWidget>

#include <memory>

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
              WRITE setLineEditEnabled
              STORED false)
   Q_PROPERTY(bool listVisible
              READ isListVisible
              WRITE setListVisible
              STORED false)
   Q_PROPERTY(QString searchText
              READ searchText
              WRITE setSearchText
              NOTIFY searchTextChanged
              USER true
              STORED false)

public:
   bool isLineEditEnabled() const;
   bool isListVisible() const;
   QString searchText() const;

public slots:
   void setLineEditEnabled(bool value);
   void setListVisible(bool value);
   void setSearchText(QString value);

signals:
   void searchTextChanged(QString searchText);
/* Properties Section End */

public:
   explicit SearchWidget(QWidget *parent = nullptr);
   ~SearchWidget() override;

   bool eventFilter(QObject *watched, QEvent *event) override;

   void init(std::shared_ptr<ChatClient> handler);

public slots:
   void clearLineEdit();
   void startListAutoHide();
   void onSearchUserListReceived(const std::vector<std::shared_ptr<Chat::Data>>& users, bool emailEntered);

private slots:
   void resetTreeView();
   void showContextMenu(const QPoint &pos);
   void focusResults();
   void closeResult();
   void onItemClicked(const QModelIndex &index);
   void leaveSearchResults();
   void leaveAndCloseSearchResults();
   void onInputTextChanged(const QString &text);
   void onSearchUserTextEdited();

signals:
   void addFriendRequied(const QString &userID);
   void showUserRoom(const QString &userID);

private:
   QScopedPointer<Ui::SearchWidget> ui_;
   QScopedPointer<QTimer>           listVisibleTimer_;

   QScopedPointer<UserSearchModel>  userSearchModel_;
   std::shared_ptr<ChatClient>      chatClient_ = nullptr;
};

#endif // SEARCHWIDGET_H
