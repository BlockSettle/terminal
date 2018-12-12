#ifndef CHATWIDGET_H
#define CHATWIDGET_H

#include <QWidget>
#include <QStringListModel>
#include <QScopedPointer>


namespace Ui {
    class ChatWidget;
}


class ChatWidget : public QWidget
{
    Q_OBJECT


private:

    QScopedPointer<Ui::ChatWidget> ui;

    QStringListModel *model;


public:

    explicit ChatWidget(QWidget *parent = nullptr);
    ~ChatWidget();

    void addLine(const QString &txt);

};

#endif // CHATWIDGET_H
