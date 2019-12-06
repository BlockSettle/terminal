/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef TOGGLESWITCH_H
#define TOGGLESWITCH_H

#include <QSlider>
class QPropertyAnimation;

class ToggleSwitch : public QSlider
{
    Q_OBJECT
public:
    explicit ToggleSwitch(QWidget *parent = nullptr);
    void setCheckState(Qt::CheckState state);
    void setChecked(bool bChecked) { setCheckState(bChecked ? Qt::Checked : Qt::Unchecked); }
    void setEnabled(bool bEnabled);
    int checkState() { return checkState_; }
    bool isChecked() { return checkState_ == Qt::Checked; }

signals:
   void stateChanged(int);
   void clicked();

public slots:

protected:
   virtual void mousePressEvent(QMouseEvent *ev);

private:
   QPropertyAnimation *animation_;
   int checkState_;
};

#endif // TOGGLESWITCH_H