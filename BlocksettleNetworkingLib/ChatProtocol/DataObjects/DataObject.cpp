#include "DataObject.h"
#include "../ProtocolDefinitions.h"

#include <map>

#include "MessageData.h"
#include "ChatRoomData.h"

namespace Chat {
   
   
   
   static std::map<std::string, DataObject::Type> DataObjectTypeFromString
   {
          { "MessageData" , DataObject::Type::MessageData }
      ,   { "ChatRoomData", DataObject::Type::ChatRoomData}
   };
   
   static std::map<DataObject::Type, std::string> DataObjectTypeToString
   {
          { DataObject::Type::MessageData , "MessageData"   }
      ,   { DataObject::Type::ChatRoomData, "ChatRoomData" }
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
      
      data[TypeKey] = QString::fromStdString(DataObjectTypeToString[type_]);
      
      return data;
   }
   
   std::string DataObject::toJsonString() const
   {
      return serializeData(this);
   }
   
   std::shared_ptr<DataObject> Chat::DataObject::fromJSON(const std::string& jsonData)
   {
      QJsonObject data = QJsonDocument::fromJson(QString::fromStdString(jsonData).toUtf8()).object();
      const DataObject::Type dataType = DataObjectTypeFromString[data[TypeKey].toString().toStdString()];
      
      switch (dataType) {
         case DataObject::Type::MessageData:
            return MessageData::fromJSON(jsonData);
         case DataObject::Type::ChatRoomData:
            return ChatRoomData::fromJSON(jsonData);
      }
      
      return nullptr;
   }
}
