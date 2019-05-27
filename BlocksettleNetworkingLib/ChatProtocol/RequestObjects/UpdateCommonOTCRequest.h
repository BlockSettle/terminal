#ifndef UPDATEOTCDATAREQUEST_H
#define UPDATEOTCDATAREQUEST_H

#include "Request.h"

namespace Chat {

   class UpdateCommonOTCRequest : public Request
   {
   public:
      UpdateCommonOTCRequest (const std::string& clientId, std::shared_ptr<OTCUpdateData> otcUpdateData);
      QJsonObject toJson() const override;
      static std::shared_ptr<Request> fromJSON(const std::string& clientId
                                     , const std::string& jsonData);
      void handle(RequestHandler &) override;

      std::shared_ptr<OTCUpdateData> getOtcUpdateData() const;

   private:
      std::shared_ptr<OTCUpdateData> otcUpdateData_;
   };
}

#endif // UPDATEOTCDATAREQUEST_H
