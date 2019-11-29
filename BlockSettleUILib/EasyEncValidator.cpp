/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "EncryptionUtils.h"
#include "EasyEncValidator.h"
#include "CoreWallet.h"

QValidator::State EasyEncValidator::validate(QString &input, int &pos) const
{
   if (input.isEmpty()) {
      setStatusMsg({});
      return QValidator::State::Intermediate;
   }

   bool lastPos = (pos == input.length());
   QString tmpInput = input.trimmed().toLower().remove(QChar::Space);
   QString newInput;
   int newPos = 0;
   const auto &allowedChars = codec_->allowedChars();
   for (int i = 0; i < tmpInput.length(); i++) {
      if ( i && i % wordSize_ == 0) {
         newInput.append(QChar::Space);
         ++newPos;
      }
      const char c = tmpInput.at(i).toLatin1();
      if ((allowedChars.find(c) == allowedChars.end())) {
         input = newInput;
         pos = newPos;
         setStatusMsg(invalidMsgTmpl_.arg(name_));
         return QValidator::State::Invalid;
      }
      newInput.append(QChar::fromLatin1(c));
      ++newPos;
   }
   input = newInput;
   if (lastPos) {
      pos = newPos;
   }

   QString inputWithoutSpaces = input;
   inputWithoutSpaces.replace(QStringLiteral(" "), QStringLiteral(""));
   if (inputWithoutSpaces.size() < numWords_ * wordSize_){
      setStatusMsg({});
      return QValidator::State::Intermediate;
   }
   if (!isValidKeyFormat(input)) {
      setStatusMsg(invalidMsgTmpl_.arg(name_));
      return QValidator::State::Intermediate;
   }
   if ((input.length() > maxLen())) {
      input = input.left(maxLen());
      pos = maxLen();
   }
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

size_t EasyEncValidator::getWordSize() const
{
   return wordSize_;
}

size_t EasyEncValidator::getNumWords() const
{
   return numWords_;
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
      bs::core::wallet::Seed::decodeEasyCodeLineChecksum(in);
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
