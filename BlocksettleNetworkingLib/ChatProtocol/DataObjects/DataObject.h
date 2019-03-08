#pragma once

#include <memory>

#include <QJsonObject>

namespace Chat {
   class DataObject {
   public:
      virtual QJsonObject toJson() = 0;
      virtual std::string toJsonString() = 0;
      static std::shared_ptr<DataObject> fromJSON(const std::string& jsonData);
      virtual ~DataObject() = default;
   };
}
