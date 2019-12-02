/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __CUSTOM_DOUBLE_SPIN_BOX_H__
#define __CUSTOM_DOUBLE_SPIN_BOX_H__

#include <QDoubleSpinBox>

class CustomDoubleSpinBox : public QDoubleSpinBox
{
public:
   explicit CustomDoubleSpinBox(QWidget *parent = nullptr);
   ~CustomDoubleSpinBox() override = default;

public:
   QValidator::State validate(QString &input, int &pos) const override;
   double valueFromText(const QString &text) const override;
   QString textFromValue(double val) const override;
   void fixup(QString &str) const override;
};

#endif // __CUSTOM_DOUBLE_SPIN_BOX_H__