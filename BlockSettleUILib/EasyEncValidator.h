/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __EASY_ENC_VALIDATOR_H__
#define __EASY_ENC_VALIDATOR_H__

#include <memory>
#include <QObject>
#include <QCoreApplication>
#include <QString>
#include <QValidator>
#include "EasyCoDec.h"


class EasyEncValidator : public QValidator
{

   Q_OBJECT
   Q_PROPERTY(QString statusMsg READ getStatusMsg NOTIFY statusMsgChanged)
   Q_PROPERTY(QString name READ getName WRITE setName)

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
      , wordSize_(wordSize)
      , numWords_(numWords)
      , hasChecksum_(hasChecksum)
      , codec_(codec)
   {}

   State validate(QString &input, int &pos) const override;
   void fixup(QString &input) const override;

   QLocale locale() const;
   void setLocale(const QLocale & locale);

   bool isValidKeyFormat(const QString &) const;
   ValidationResult validateKey(const QString &) const;

   QString getStatusMsg() const;
   void setStatusMsg(const QString &getStatusMsg) const;

   QString getName() const;
   void setName(const QString &getName);


   size_t getWordSize() const;
   size_t getNumWords() const;

signals:
   void statusMsgChanged(const QString& newStatusMsg) const;

private:
   const size_t   wordSize_;
   const size_t   numWords_;
   const bool     hasChecksum_;
   std::shared_ptr<EasyCoDec> codec_;
   mutable QString statusMsg_;
   QString name_ = QString::fromStdString("Line");

   QString validMsgTmpl_ = QString::fromStdString("%1 ") + QCoreApplication::translate("", "Valid");
   QString invalidMsgTmpl_ = QCoreApplication::translate("", "Wrong checksum in ") + QString::fromStdString(" %1");

private:
   int maxLen() const { return wordSize_ * numWords_ + numWords_ - 1; }
   ValidationResult validateChecksum(const std::string &) const;
};


#endif // __EASY_ENC_VALIDATOR_H__
