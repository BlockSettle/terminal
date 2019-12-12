/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __CUSTOM_DOUBLE_VALIDATOR_H__
#define __CUSTOM_DOUBLE_VALIDATOR_H__

#include <QDoubleValidator>
#include <QByteArray>

class CustomDoubleValidator : public QDoubleValidator
{
public:
   CustomDoubleValidator(QObject* parent);
   ~CustomDoubleValidator() noexcept override = default;

   State validate(QString &input, int &pos) const override;
   void setDecimals(int decimals);

   double GetValue(const QString& input, bool *ok = nullptr) const;

private:
   int      decimals_;
};

#endif // __CUSTOM_DOUBLE_VALIDATOR_H__
