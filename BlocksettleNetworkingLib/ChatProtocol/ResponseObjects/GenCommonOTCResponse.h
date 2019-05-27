#ifndef SUBMITOTCRESPONSE_H
#define SUBMITOTCRESPONSE_H

#include "Response.h"

namespace Chat {
   class GenCommonOTCResponse : public Response
   {
   public:
      GenCommonOTCResponse(std::shared_ptr<OTCRequestData> otcRequestData,
                        OTCRequestResult result = OTCRequestResult::Rejected,
                        const QString& message =
            QLatin1String("The server did not provide the message."));

      QJsonObject toJson() const override;
      static std::shared_ptr<Response> fromJSON(const std::string& jsonData);
      void handle(ResponseHandler &) override;
      std::shared_ptr<OTCRequestData> otcRequestData() const;
      OTCRequestResult getResult() const;
      QString getMessage() const;

      void setMessage(const QString &message);

   private:
      std::shared_ptr<OTCRequestData> otcRequestData_;
      OTCRequestResult result_;
      QString message_;
   };
}

#endif // SUBMITOTCESPONSE_H
