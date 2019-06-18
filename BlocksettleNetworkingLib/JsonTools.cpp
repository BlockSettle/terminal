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
}
