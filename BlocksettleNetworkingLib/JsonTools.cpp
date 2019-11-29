/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "JsonTools.h"

#include <QJsonValue>
#include <QString>

namespace JsonTools
{
   double GetDouble(const QJsonValue& value, bool& converted)
   {
      if (value.isDouble()) {
         converted = true;
         return value.toDouble();
      } else {
         return value.toString().toDouble(&converted);
      }
   }

   QString GetStringProperty(const QVariantMap& settingsMap, const QString& propertyName)
   {
      if (settingsMap.contains(propertyName)) {
         const auto value = settingsMap.value(propertyName);

         if (value.isValid()) {
            return value.toString();
         }
      }

      return QString{};
   }

   double GetDoubleProperty(const QVariantMap& settingsMap, const QString& propertyName, bool *converted)
   {
      auto it = settingsMap.constFind(propertyName);
      if (it == settingsMap.constEnd()) {
         if (converted != nullptr) {
            *converted = false;
         }
         return 0;
      }

      return it->toDouble(converted);
   }

   bool LoadStringFields(const QVariantMap& data, std::vector<std::pair<QString, std::string*>>& fields
                         , std::string &errorMessage, const FieldsLoadingRule loadingRule)
   {
      for (auto& fieldInfo : fields) {
         const auto it = data.constFind(fieldInfo.first);
         if ((it == data.constEnd()) || (!it->isValid())) {
            if (loadingRule == FieldsLoadingRule::NonEmptyOnly) {
               errorMessage = "Field not found: " + fieldInfo.first.toStdString();
               return false;
            }

            // set empty string
            *fieldInfo.second = std::string();
         } else {
            *fieldInfo.second = it->toString().toStdString();
            if (fieldInfo.second->empty() && (loadingRule == FieldsLoadingRule::NonEmptyOnly)) {
               errorMessage = "Field empty: " + fieldInfo.first.toStdString();
               return false;
            }
         }
      }

      return true;
   }

   bool LoadIntFields(const QVariantMap& data, std::vector<std::pair<QString, int64_t*>>& fields
                      , std::string &errorMessage, const FieldsLoadingRule loadingRule)
   {
      for (auto& fieldInfo : fields) {
         const auto it = data.constFind(fieldInfo.first);
         if ((it == data.constEnd()) || (!it->isValid())) {
            if (loadingRule == FieldsLoadingRule::NonEmptyOnly) {
               errorMessage = "Field not found: " + fieldInfo.first.toStdString();
               return false;
            }

            // set empty string
            *fieldInfo.second = 0;
         } else {
            bool converted = false;
            *fieldInfo.second = it->toInt(&converted);

            if (!converted) {
               errorMessage = "Invalid value for field: " + fieldInfo.first.toStdString();
               return false;
            }
         }
      }

      return true;
   }

   bool LoadDoubleFields(const QVariantMap& data, std::vector<std::pair<QString, double*>>& fields
                         , std::string &errorMessage, const FieldsLoadingRule loadingRule)
   {
      for (auto& fieldInfo : fields) {
         const auto it = data.constFind(fieldInfo.first);
         if ((it == data.constEnd()) || (!it->isValid())) {
            if (loadingRule == FieldsLoadingRule::NonEmptyOnly) {
               errorMessage = "Field not found: " + fieldInfo.first.toStdString();
               return false;
            }

            // set empty string
            *fieldInfo.second = 0;
         } else {
            bool converted = false;
            *fieldInfo.second = it->toDouble(&converted);

            if (!converted) {
               errorMessage = "Invalid value for field: " + fieldInfo.first.toStdString();
               return false;
            }
         }
      }

      return true;
   }

}
