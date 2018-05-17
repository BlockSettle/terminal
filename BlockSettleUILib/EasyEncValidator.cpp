#include "EncryptionUtils.h"
#include "EasyEncValidator.h"
#include "MetaData.h"

QValidator::State EasyEncValidator::validate(QString &input, int &pos) const
{
   if (input.isEmpty()) {
      setStatusMsg({});
      return QValidator::State::Intermediate;
   }
   input = input.toLower();
   const auto &allowedChars = codec_->allowedChars();
   for (int i = 0; i < input.length(); i++) {
      const char c = input.at(i).toLatin1();
      if ((allowedChars.find(c) == allowedChars.end()) && (c != ' ')) {
         pos = i;
         setStatusMsg(invalidMsgTmpl_.arg(name_));
         return QValidator::State::Invalid;
      }

      if (((i % (wordSize_ + 1)) == (wordSize_ - 1)) && ((i + 1) < maxLen())) {
         if (((i + 1) >= input.length()) && (pos == (i + 1) && (prevPos_ <= pos))) {
            input.append(QLatin1Char(' '));
            ++pos;
         }
      }
   }
   if (!isValidKeyFormat(input)) {
      setStatusMsg(invalidMsgTmpl_.arg(name_));
      return QValidator::State::Intermediate;
   }
   if ((input.length() > maxLen()) && (prevPos_ == (pos - 1))) {
      input = input.left(maxLen());
      pos = maxLen();
   }
   prevPos_ = pos;
   if (input.length() == maxLen()) {
       if (validateKey(input) == Valid) {
           setStatusMsg(validMsgTmpl_.arg(name_));
           return QValidator::State::Acceptable;
       }
   }

   setStatusMsg(invalidMsgTmpl_.arg(name_));
   return QValidator::State::Intermediate;
}

bool EasyEncValidator::isValidKeyFormat(const QString &input) const
{
   for (int i = 0; i < qMin<int>(input.length(), maxLen()); i++) {
      const char c = input.at(i).toLatin1();
      const auto iRem = (i % (wordSize_ + 1));
      if ((iRem < wordSize_) && (c == ' ')) {
         return false;
      }
      if ((iRem == wordSize_) && (c != ' ')) {
         return false;
      }
   }
   return true;
}

EasyEncValidator::ValidationResult EasyEncValidator::validateKey(const QString &input) const
{
   if (!isValidKeyFormat(input)) {
      return InvalidFormat;
   }
   if (input.length() != maxLen()) {
      return InvalidLength;
   }
   if (hasChecksum_) {
      if (numWords_ < 2) {
         return InvalidLength;
      }
      return validateChecksum(input.toStdString());
   }
   return Valid;
}

QString EasyEncValidator::getStatusMsg() const
{
   return statusMsg_;
}

QString EasyEncValidator::getName() const
{
   return name_;
}

void EasyEncValidator::setName(const QString &name)
{
   name_ = name;
}

void EasyEncValidator::setStatusMsg(const QString &statusMsg) const
{
   statusMsg_ = statusMsg;
   emit statusMsgChanged(statusMsg);
}

EasyEncValidator::ValidationResult EasyEncValidator::validateChecksum(const std::string &in) const
{
   try {
      bs::wallet::Seed::decodeEasyCodeLineChecksum(in);
        return Valid;
    } catch (...) {
        return InvalidChecksum;
    }
}

void EasyEncValidator::fixup(QString &input) const
{
   if (isValidKeyFormat(input)) {
      input = input.left(maxLen());
      return;
   }
   int i = 0;
   while (i < input.length()) {
      const char c = input.at(i).toLatin1();
      const auto iRem = (i % (wordSize_ + 1));
      if ((iRem < wordSize_) && (c == ' ')) {
         input.remove(i, 1);
         continue;
      }
      else if ((iRem == wordSize_) && (c != ' ')) {
         input.insert(i, QLatin1Char(' '));
      }
      i++;
   }
   if (input.length() > maxLen()) {
      input = input.left(maxLen());
   }
}

QLocale EasyEncValidator::locale() const
{
    return QValidator::locale();
}

void EasyEncValidator::setLocale(const QLocale &locale)
{
    QValidator::setLocale(locale);
}
