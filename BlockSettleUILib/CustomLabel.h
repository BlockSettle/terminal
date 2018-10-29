#ifndef CUSTOMLABEL_H
#define CUSTOMLABEL_H

#include <QLabel>

class CustomLabel : public QLabel
{
    Q_OBJECT
public:
    CustomLabel(QWidget *parent);

protected:
   void mouseReleaseEvent(QMouseEvent *ev);

};

#endif // CUSTOMLABEL_H
