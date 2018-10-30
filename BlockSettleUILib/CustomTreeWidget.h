#ifndef CUSTOMTREEWIDGET_H
#define CUSTOMTREEWIDGET_H

#include <QTreeWidget>

class CustomTreeWidget : public QTreeWidget
{
    Q_OBJECT
public:
    CustomTreeWidget(QWidget *parent = nullptr);

signals:


protected:
   void mouseReleaseEvent(QMouseEvent *ev);
};

#endif // CUSTOMTREEWIDGET_H