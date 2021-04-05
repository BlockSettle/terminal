/*

***********************************************************************************
* Copyright (C) 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "Bip39EntryValidator.h"
#include "Bip39.h"
#include "QmlFactory.h"

Bip39EntryValidator::Bip39EntryValidator(QObject *parent)
   : QValidator(parent)
{
}

void Bip39EntryValidator::initDictionaries(QmlFactory* factory)
{
   dictionaries_ = factory->bip39Dictionaries();
}

QValidator::State Bip39EntryValidator::validate(QString &input, int &pos) const
{
   if (dictionaries_.empty() || input.isEmpty()) {
      return State::Invalid;
   }

   auto qWordsList = input.trimmed().split(QRegExp(QLatin1String("\\s+")));
   if (qWordsList.size() != wordsCount_) {
      return State::Intermediate;
   }

   if (!validateMnemonic(input.toStdString(), dictionaries_)) {
      return State::Invalid;
   }

   return State::Acceptable;
}

bool Bip39EntryValidator::validate(QString input)
{
   int pos = 0;
   return validate(input, pos) == QValidator::State::Acceptable;
}
