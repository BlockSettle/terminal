#ifndef SEARCHWIDGET_H
#define SEARCHWIDGET_H

#include <QWidget>

namespace Ui {
   class SearchWidget;
}

class SearchWidget : public QWidget
{
   Q_OBJECT
public:
   explicit SearchWidget(QWidget *parent = nullptr);
   ~SearchWidget() override;

private:
   QScopedPointer<Ui::SearchWidget> ui_;
};

#endif // SEARCHWIDGET_H
