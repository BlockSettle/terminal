/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __JSON_TOOLS_H__
#define __JSON_TOOLS_H__

#include <QString>
#include <QVariantMap>

#include <string>
#include <vector>

class QJsonValue;

namespace JsonTools
{
   // converted set to true if double was extracted successfully
   double GetDouble(const QJsonValue& value, bool& converted);

   QString GetStringProperty(const QVariantMap& settingsMap, const QString& propertyName);
   double  GetDoubleProperty(const QVariantMap& settingsMap, const QString& propertyName, bool *converted = nullptr);

   enum class FieldsLoadingRule
   {
      NonEmptyOnly,
      EmptyAllowed
   };

   bool LoadStringFields(const QVariantMap& data, std::vector<std::pair<QString, std::string*>>& fields
                         , std::string &errorMessage, const FieldsLoadingRule loadingRule = FieldsLoadingRule::NonEmptyOnly);
   bool LoadIntFields(const QVariantMap& data, std::vector<std::pair<QString, int64_t*>>& fields
                      , std::string &errorMessage, const FieldsLoadingRule loadingRule = FieldsLoadingRule::NonEmptyOnly);
   bool LoadDoubleFields(const QVariantMap& data, std::vector<std::pair<QString, double*>>& fields
                         , std::string &errorMessage, const FieldsLoadingRule loadingRule = FieldsLoadingRule::NonEmptyOnly);
}

#endif // __JSON_TOOLS_H__
