#ifndef SUBMITOTCRESPONSE_H
#define SUBMITOTCRESPONSE_H

#include "Response.h"

namespace Chat {
   class GenCommonOTCResponse : public Response
   {
   public:
      GenCommonOTCResponse(std::shared_ptr<OTCRequestData> otcRequestData,
                        OTCResult result = OTCResult::Rejected,
                        const QString& message =
            QLatin1String("The server did not provide the message."));

      QJsonObject toJson() const override;
      static std::shared_ptr<Response> fromJSON(const std::string& jsonData);
      void handle(ResponseHandler &) override;
      std::shared_ptr<OTCRequestData> otcRequestData() const;
      OTCResult getResult() const;
      QString getMessage() const;
      void setMessage(const QString &message);

   private:
      std::shared_ptr<OTCRequestData> otcRequestData_;
      OTCResult result_;
      QString message_;
   };
}

#endif // SUBMITOTCESPONSE_H
