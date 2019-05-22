#ifndef SENDOTCDATARESPONSE_H
#define SENDOTCDATARESPONSE_H

#include "Response.h"

namespace Chat {
   class SendOTCDataResponse : public Response
   {
   public:
      SendOTCDataResponse(std::shared_ptr<OTCResponseData> otcResponseData);
      QJsonObject toJson() const override;
      static std::shared_ptr<Response> fromJSON(const std::string& jsonData);
      void handle(ResponseHandler &) override;
      std::shared_ptr<OTCResponseData> otcResponseData() const;

   private:
      std::shared_ptr<OTCResponseData> otcResponseData_;
   };
}

#endif // SENDOTCDATARESPONSE_H
