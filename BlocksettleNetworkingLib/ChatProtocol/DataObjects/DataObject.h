#ifndef DataObject_h__
#define DataObject_h__

#include <memory>

#include <QJsonObject>

namespace Chat {

   class DataObject {
   public:
      enum class Type {
            MessageData,
            RoomData,
            ContactRecordData,
            UserData
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

#endif // DataObject_h__
