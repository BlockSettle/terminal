/*

***********************************************************************************
* Copyright (C) 2020 - 2021, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __BIP_39_ENTRY_VALIDATOR_H__
#define __BIP_39_ENTRY_VALIDATOR_H__

#include <QValidator>

class QmlFactory;
class Bip39EntryValidator : public QValidator
{
   Q_OBJECT
   Q_PROPERTY(int wordsCount MEMBER wordsCount_)
public:
   Bip39EntryValidator(QObject *parent = nullptr);
   ~Bip39EntryValidator() override = default;

   QValidator::State validate(QString &input, int &pos) const override;
   Q_INVOKABLE void initDictionaries(QmlFactory* factory);
   Q_INVOKABLE bool validate(QString input);
signals:
   void isValidChanged();

private:
   std::vector<std::vector<std::string>> dictionaries_;
   int wordsCount_;
};

#endif // __BIP_39_ENTRY_VALIDATOR_H__
