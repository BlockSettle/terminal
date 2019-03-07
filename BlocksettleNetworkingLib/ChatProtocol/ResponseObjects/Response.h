#pragma once

#include "../ProtocolDefinitions.h"
#include "../ResponseHandler.h"

namespace Chat {
   
   template <typename T>
   QJsonObject Message<T>::toJson() const
   {
      QJsonObject data;
   
      data[VersionKey] = QString::fromStdString(version_);
   
      return data;
   }
   
   class Response : public Message<ResponseType>
   {
   public:

      Response(ResponseType responseType)
         : Message<ResponseType> (responseType)
      {
      }
      ~Response() override = default;
      virtual std::string getData() const;
      QJsonObject toJson() const override;
      static std::shared_ptr<Response> fromJSON(const std::string& jsonData);
      virtual void handle(ResponseHandler &) = 0;
     ResponseType getType() { return messageType_; }
   };
   
   class PendingResponse : public Response
   {
   public:
      PendingResponse(ResponseType type, const QString &id = QString());
      QJsonObject toJson() const override;
      QString getId() const;
      void setId(const QString& id);
      void handle(ResponseHandler &) override;
   private:
      QString id_;
      
   };
   
}
