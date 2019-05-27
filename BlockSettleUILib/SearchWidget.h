#ifndef SEARCHWIDGET_H
#define SEARCHWIDGET_H

#include <QWidget>

#include <memory>

class ListModel;

namespace Ui {
   class SearchWidget;
}

namespace Chat {
   class UserData;
}

class SearchWidget : public QWidget
{
   Q_OBJECT
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
   explicit SearchWidget(QWidget *parent = nullptr);
   ~SearchWidget() override;

   bool eventFilter(QObject *watched, QEvent *event) override;

   void init();

   bool isLineEditEnabled() const;
   bool isListVisible() const;
   QString searchText() const;
   void setUsers(const std::vector<std::shared_ptr<Chat::UserData>> &users);

public slots:
   void clearLineEdit();
   void clearList();
   void startListAutoHide();
   void setLineEditEnabled(bool value);
   void setListVisible(bool value);
   void setSearchText(QString value);

private slots:
   void showContextMenu(const QPoint &pos);

signals:
   void searchUserTextEdited(const QString &text);
   void searchTextChanged(QString searchText);
   void addFriendRequied(const QString &userID);
   void removeFriendRequired(const QString &userID);

private:
   QScopedPointer<Ui::SearchWidget> ui_;
   QScopedPointer<QTimer> listVisibleTimer_;
   QScopedPointer<ListModel> model_;
};

#endif // SEARCHWIDGET_H
