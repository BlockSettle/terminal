#ifndef __JSON_TOOLS_H__
#define __JSON_TOOLS_H__

#include <QString>
#include <QVariantMap>

class QJsonValue;

namespace JsonTools
{
   // converted set to true if double was extracted successfully
   double GetDouble(const QJsonValue& value, bool& converted);

   QString GetStringProperty(const QVariantMap& settingsMap, const QString& propertyName);
}

#endif // __JSON_TOOLS_H__