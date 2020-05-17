/*

***********************************************************************************
* Copyright (C) 2016 - , BlockSettle AB
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

   std::vector<std::string> words;
   words.reserve(qWordsList.size());
   for (auto qWord : qWordsList) {
      words.push_back(qWord.toStdString());
   }

   for (const auto& dict : dictionaries_) {
      if (validate_mnemonic(words, dict)) {
         return State::Acceptable;
      }
   }

   return State::Invalid;
}

bool Bip39EntryValidator::validate(QString input)
{
   int pos = 0;
   return validate(input, pos) == QValidator::State::Acceptable;
}
