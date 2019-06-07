#ifndef SUBMITOTCREQUEST_H
#define SUBMITOTCREQUEST_H
#include "Request.h"

namespace Chat {
   class GenCommonOTCRequest : public Request
   {
   public:
      GenCommonOTCRequest(const std::string &clientId, std::shared_ptr<OTCRequestData> otcData);
      QJsonObject toJson() const override;
      static std::shared_ptr<Request> fromJSON(const std::string& clientId
                                               , const std::string& jsonData);
      void handle(RequestHandler&) override;
      std::shared_ptr<OTCRequestData> getOtcRequestData() const;
      std::string getSenderId() const;

   private:
      std::shared_ptr<OTCRequestData> otcRequestData_;
   };
}

#endif // SUBMITOTCREQUEST_H
