/*

***********************************************************************************
* Copyright (C) 2016 - , BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "PasswordConfirmValidator.h"

PasswordConfirmValidator::PasswordConfirmValidator(QObject* parent) :
   QValidator(parent)
{

}

QValidator::State PasswordConfirmValidator::validate(QString &input, int &) const
{
   // accept only ascii chars
   for (const QChar &c : input) {
      if (c.unicode() <= 31 || c.unicode() >= 127) {
         setStatusMsg(invalidCharTmpl_.arg(name_));
         return QValidator::State::Invalid;
      }
   }

   if (!input.isEmpty() && input.size() < 6) {
      setStatusMsg(tooShortTmpl_.arg(name_));
      return QValidator::State::Intermediate;
   }

   if (input.size() == compareTo_.size() && input.size() >= 6) {
      if (input == compareTo_) {
         setStatusMsg(validTmpl_.arg(name_));
         return QValidator::State::Acceptable;
      }
      else {
         setStatusMsg(dontMatchMsgTmpl_.arg(name_));
         return  QValidator::State::Intermediate;
      }
   }

   if (input.size() != compareTo_.size() && !compareTo_.isEmpty()) {
      setStatusMsg(dontMatchMsgTmpl_.arg(name_));
      return QValidator::State::Intermediate;
   }

   if (input.size() >= 6 || compareTo_.isEmpty()) {
      setStatusMsg({});
      return QValidator::State::Intermediate;
   }

   //control should never get here, but just in case:
   return QValidator::State::Intermediate;
}

QLocale PasswordConfirmValidator::locale() const
{
   return QValidator::locale();
}

void PasswordConfirmValidator::setLocale(const QLocale &locale)
{
   QValidator::setLocale(locale);
}

QString PasswordConfirmValidator::getCompareTo() const
{
   return compareTo_;
}

void PasswordConfirmValidator::setCompareTo(const QString &compareTo)
{
   compareTo_ = compareTo;
   emit QValidator::changed();
}

QString PasswordConfirmValidator::getStatusMsg() const
{
   return statusMsg_;
}

void PasswordConfirmValidator::setStatusMsg(const QString &error) const
{
   statusMsg_ = error;
   emit statusMsgChanged(error);
}

QString PasswordConfirmValidator::getName() const
{
   return name_;
}

void PasswordConfirmValidator::setName(const QString &name)
{
   name_ = name;
}
