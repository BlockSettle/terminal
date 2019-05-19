#pragma once
#include <memory>

#include <QJsonObject>

namespace Chat {

   class DataObject {
   public:
      enum class Type {
            MessageData,
            RoomData,
            ContactRecordData,
            UserData,
            OTCRequestData,
            OTCResponseData,
            OTCUpdateData
         };
   protected:
      DataObject(Type type);
   public:
      virtual ~DataObject() = default;

      Type getType() const;
      virtual QJsonObject toJson() const;
      std::string toJsonString() const;

      static std::shared_ptr<DataObject> fromJSON(const std::string& jsonData);
   private:
      Type type_;
   };
}
