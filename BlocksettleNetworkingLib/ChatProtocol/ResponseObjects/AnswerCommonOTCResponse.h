#ifndef ANSWERCOMMONOTCRESPONSE_H
#define ANSWERCOMMONOTCRESPONSE_H

#include "Response.h"

namespace Chat {
   class AnswerCommonOTCResponse : public Response
   {
   public:
      AnswerCommonOTCResponse(std::shared_ptr<OTCResponseData> otcResponseData,
                              OTCResult result = OTCResult::Rejected,
                              const QString& message =
                  QLatin1String("The server did not provide the message."));

      QJsonObject toJson() const override;
      static std::shared_ptr<Response> fromJSON(const std::string& jsonData);
      void handle(ResponseHandler &) override;
      std::shared_ptr<OTCResponseData> otcResponseData() const;
      OTCResult getResult() const;
      QString getMessage() const;
      void setMessage(const QString &message);

   private:
      std::shared_ptr<OTCResponseData> otcResponseData_;
      OTCResult result_;
      QString message_;
   };
}

#endif // ANSWERCOMMONOTCRESPONSE_H
