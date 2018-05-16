#ifndef __EASY_ENC_VALIDATOR_H__
#define __EASY_ENC_VALIDATOR_H__

#include <memory>
#include <QValidator>
#include "EasyCoDec.h"


class EasyEncValidator : public QValidator
{
public:
   enum ValidationResult {
      Valid,
      InvalidFormat,
      InvalidLength,
      InvalidChecksum
   };

   //contructor used by the QmlEngine
   explicit EasyEncValidator(QObject *parent = nullptr) :
       QValidator(parent),
       wordSize_(4),
       numWords_(9),
       hasChecksum_(true),
       codec_(std::make_shared<EasyCoDec>())
   {}

   EasyEncValidator(const std::shared_ptr<EasyCoDec> &codec, QObject *parent = nullptr
      , size_t numWords = 8, bool hasChecksum = false, size_t wordSize = 4)
      : QValidator(parent)
      , numWords_(numWords), wordSize_(wordSize), hasChecksum_(hasChecksum), codec_(codec)
   {}

   State validate(QString &input, int &pos) const override;
   void fixup(QString &input) const override;

   QLocale locale() const;
   void setLocale(const QLocale & locale);

   bool isValidKeyFormat(const QString &) const;
   ValidationResult validateKey(const QString &) const;

private:
   const size_t   wordSize_;
   const size_t   numWords_;
   const bool     hasChecksum_;
   std::shared_ptr<EasyCoDec> codec_;
   mutable int    prevPos_ = 0;

private:
   constexpr int maxLen() const { return wordSize_ * numWords_ + numWords_ - 1; }
   ValidationResult validateChecksum(const std::string &) const;
};


#endif // __EASY_ENC_VALIDATOR_H__
