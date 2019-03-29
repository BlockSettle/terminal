#pragma once
#include <memory>

#include <QJsonObject>

namespace Chat {
   
   class DataObject {
   public:
      enum class Type {
            MessageData,
            ChatRoomData,
            ContactRecordData,
            ChatUserData
         };
   protected:
      DataObject(Type type);
   public:
      Type getType() const;
      virtual QJsonObject toJson() const;
      std::string toJsonString() const;
      static std::shared_ptr<DataObject> fromJSON(const std::string& jsonData);
      virtual ~DataObject() = default;
   private:
      Type type_;
   };
}
