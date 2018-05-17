#include "PasswordConfirmValidator.h"

PasswordConfirmValidator::PasswordConfirmValidator(QObject* parent) :
   QValidator(parent)
{

}

QValidator::State PasswordConfirmValidator::validate(QString &input, int &) const
{
   if (input.isEmpty() || compareTo_.isEmpty()) {
      setStatusMsg({});
      return QValidator::State::Intermediate;
   }

   if (input.size() < compareTo_.size()) {
      setStatusMsg(dontMatchMsgTmpl_.arg(name_));
      return QValidator::State::Intermediate;
   }

   if ( input.size() == compareTo_.size()) {
      if (input == compareTo_) {
         setStatusMsg(validTmpl_.arg(name_));
         return QValidator::State::Acceptable;
      } else {
         setStatusMsg(dontMatchMsgTmpl_.arg(name_));
         return  QValidator::State::Intermediate;
      }
   }

   if (input.size() > compareTo_.size()) {
      setStatusMsg(tooLongTmpl_.arg(name_));
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
