#include "DataObject.h"
#include "../ProtocolDefinitions.h"

#include "ContactRecordData.h"
#include "MessageData.h"
#include "OTCRequestData.h"
#include "OTCResponseData.h"
#include "OTCUpdateData.h"
#include "RoomData.h"
#include "UserData.h"

#include <map>

namespace Chat {
   static const std::map<std::string, DataObject::Type> DataObjectTypeFromString
   {
          { "MessageData" , DataObject::Type::MessageData }
      ,   { "RoomData", DataObject::Type::RoomData}
      ,   { "ContactRecordData", DataObject::Type::ContactRecordData}
      ,   { "UserData", DataObject::Type::UserData}
      ,   { "OTCRequestData", DataObject::Type::OTCRequestData }
      ,   { "OTCResponseData", DataObject::Type::OTCResponseData }
      ,   { "OTCUpdateData", DataObject::Type::OTCUpdateData }
   };

   static const std::map<DataObject::Type, std::string> DataObjectTypeToString
   {
          { DataObject::Type::MessageData , "MessageData"   }
      ,   { DataObject::Type::RoomData, "RoomData" }
      ,   { DataObject::Type::ContactRecordData, "ContactRecordData" }
      ,   { DataObject::Type::UserData, "UserData" }
      ,   { DataObject::Type::OTCRequestData, "OTCRequestData" }
      ,   { DataObject::Type::OTCResponseData, "OTCResponseData" }
      ,   { DataObject::Type::OTCUpdateData, "OTCUpdateData" }
   };

   DataObject::DataObject(DataObject::Type type)
      :type_(type)
   {

   }

   DataObject::Type DataObject::getType() const
   {
      return type_;
   }

   QJsonObject DataObject::toJson() const
   {
      QJsonObject data;

      const auto it = DataObjectTypeToString.find(type_);
#ifndef NDEBUG
      assert(it != DataObjectTypeToString.end());
#endif

      data[TypeKey] = QString::fromStdString(it->second);

      return data;
   }

   std::string DataObject::toJsonString() const
   {
      return serializeData(this);
   }

   std::shared_ptr<DataObject> Chat::DataObject::fromJSON(const std::string& jsonData)
   {
      QJsonObject data = QJsonDocument::fromJson(QString::fromStdString(jsonData).toUtf8()).object();

      const auto it = DataObjectTypeFromString.find(data[TypeKey].toString().toStdString());
#ifndef NDEBUG
      assert(it != DataObjectTypeFromString.end());
#endif
      const DataObject::Type dataType = it->second;

      switch (dataType) {
         case DataObject::Type::MessageData:
            return MessageData::fromJSON(jsonData);
         case DataObject::Type::RoomData:
            return RoomData::fromJSON(jsonData);
         case DataObject::Type::ContactRecordData:
            return ContactRecordData::fromJSON(jsonData);
         case DataObject::Type::UserData:
            return UserData::fromJSON(jsonData);
         case DataObject::Type::OTCRequestData:
            return OTCRequestData::fromJSON(jsonData);
         case DataObject::Type::OTCResponseData:
            return OTCResponseData::fromJSON(jsonData);
         case DataObject::Type::OTCUpdateData:
            return OTCUpdateData::fromJSON(jsonData);
      }

      return nullptr;
   }
}
