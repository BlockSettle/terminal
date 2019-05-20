#ifndef SENDOTCDATARESPONSE_H
#define SENDOTCDATARESPONSE_H

#include "Response.h"

namespace Chat {
   class SendOTCDataResponse : public Response
   {
   public:
      SendOTCDataResponse(std::shared_ptr<OTCResponseData> otcResponseData);
      static std::shared_ptr<Response> fromJSON(const std::string& jsonData);
      void handle(ResponseHandler &) override;
      QJsonObject toJson() const override;
   private:
      std::shared_ptr<OTCResponseData> otcData_;
   };
}

#endif // SENDOTCDATARESPONSE_H
