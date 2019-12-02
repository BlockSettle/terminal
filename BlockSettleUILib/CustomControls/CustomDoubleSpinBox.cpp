/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "CustomDoubleSpinBox.h"

#include "UiUtils.h"

CustomDoubleSpinBox::CustomDoubleSpinBox(QWidget *parent)
 : QDoubleSpinBox(parent)
{}

QValidator::State CustomDoubleSpinBox::validate(QString &input, int &pos) const
{
   return UiUtils::ValidateDoubleString(input, pos, decimals());
}

double CustomDoubleSpinBox::valueFromText(const QString &text) const
{
   QString tempCopy = UiUtils::NormalizeString(text);
   bool converted = false;
   double value = QLocale().toDouble(tempCopy, &converted);
   if (!converted) {
      return 0;
   }

   return value;
}

QString CustomDoubleSpinBox::textFromValue(double val) const
{
   return UiUtils::UnifyValueString(QLocale().toString(val, 'f', decimals()));
}

void CustomDoubleSpinBox::fixup(QString &str) const
{
   UiUtils::NormalizeString(str);
}