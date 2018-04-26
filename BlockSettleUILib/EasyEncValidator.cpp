#include "BinaryData.h"
#include "EncryptionUtils.h"
#include "EasyEncValidator.h"


QValidator::State EasyEncValidator::validate(QString &input, int &pos) const
{
   if (input.isEmpty()) {
      return QValidator::State::Intermediate;
   }
   input = input.toLower();
   const auto &allowedChars = codec_->allowedChars();
   for (int i = 0; i < input.length(); i++) {
      const char c = input.at(i).toLatin1();
      if ((allowedChars.find(c) == allowedChars.end()) && (c != ' ')) {
         pos = i;
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
      return QValidator::State::Intermediate;
   }
   if ((input.length() > maxLen()) && (prevPos_ == (pos - 1))) {
      input = input.left(maxLen());
      pos = maxLen();
   }
   prevPos_ = pos;
   if (input.length() == maxLen()) {
      return QValidator::State::Acceptable;
   }
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

EasyEncValidator::ValidationResult EasyEncValidator::validateChecksum(const std::string &in) const
{
   const auto data = BinaryData::CreateFromHex(EasyCoDec().toHex(in));
   if (data.getSize() != (numWords_ * 2)) {
      return InvalidFormat;
   }
   const auto dataSize = (numWords_ - 1) * 2;
   const auto &hash = data.getSliceCopy(dataSize, 2);
   if (BtcUtils::getHash256(data.getSliceCopy(0, dataSize)).getSliceCopy(0, 2) != hash) {
      return InvalidChecksum;
   }
   return Valid;
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
