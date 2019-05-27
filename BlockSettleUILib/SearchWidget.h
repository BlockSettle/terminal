#ifndef SEARCHWIDGET_H
#define SEARCHWIDGET_H

#include <QWidget>

namespace Ui {
   class SearchWidget;
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

   void init();

   bool isLineEditEnabled() const;
   bool isListVisible() const;
   QString searchText() const;

public slots:
   void clearLineEdit();
   void startListAutoHide();
   void setLineEditEnabled(bool value);
   void setListVisible(bool value);
   void setSearchText(QString value);

signals:
   void searchUserTextEdited(const QString &text);
   void searchTextChanged(QString searchText);

private:
   QScopedPointer<Ui::SearchWidget> ui_;
   QScopedPointer<QTimer> listVisibleTimer_;
};

#endif // SEARCHWIDGET_H
