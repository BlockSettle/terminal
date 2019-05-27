#ifndef UPDATEOTCDATARESPONSE_H
#define UPDATEOTCDATARESPONSE_H

#include "Response.h"

namespace Chat {
   class UpdateCommonOTCResponse : public Response
   {
   public:
      UpdateCommonOTCResponse(std::shared_ptr<OTCUpdateData> otcUpdateData);
      QJsonObject toJson() const override;
      static std::shared_ptr<Response> fromJSON(const std::string& jsonData);
      void handle(ResponseHandler&) override;
      std::shared_ptr<OTCUpdateData> getOtcUpdateData() const;

   private:
      std::shared_ptr<OTCUpdateData> otcUpdateData_;

   };
}

#endif // UPDATEOTCDATARESPONSE_H
