/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef CUSTOMLABEL_H
#define CUSTOMLABEL_H

#include <QLabel>

// This custom label class supports copy to clipboard by
// right click of the mouse. To activate this functionality
// you must set 'copyToClipboard' property to be true. 
//
// It also allows to show a tooltip immediately after mouse hovers
// over it. To enable this feature you must set 'showToolTipQuickly'
// property to true and set toolTip_ member variable to the tooltip
// text. You can still use original Qt toolTip functionality but this
// delays showing the tooltip after mouse hovers over the label.
class CustomLabel : public QLabel
{
    Q_OBJECT
public:
    CustomLabel(QWidget *parent = nullptr);
    QString toolTip_;

protected:
   void mouseReleaseEvent(QMouseEvent *ev);
   void mouseMoveEvent(QMouseEvent *ev);

};

#endif // CUSTOMLABEL_H
