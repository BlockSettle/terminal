#ifndef SENDOTCDATAREQUEST_H
#define SENDOTCDATAREQUEST_H
#include "Request.h"

namespace Chat {
   class SendOTCDataRequest : public Request
   {
   public:
      SendOTCDataRequest(const std::string &clientId, std::shared_ptr<OTCRequestData> otcData);
      QJsonObject toJson() const override;
      static std::shared_ptr<Request> fromJSON(const std::string& clientId
                                     , const std::string& jsonData);
      void handle(RequestHandler&) override;
      const std::shared_ptr<OTCRequestData> getOtcRequestData() const;
   private:
      std::shared_ptr<OTCRequestData> otcData_;
   };
}

#endif // SENDOTCDATAREQUEST_H
