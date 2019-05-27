#ifndef ANSWERCOMMONOTCRESPONSE_H
#define ANSWERCOMMONOTCRESPONSE_H

#include "Response.h"

namespace Chat {
   class AnswerCommonOTCResponse : public Response
   {
   public:
      AnswerCommonOTCResponse(std::shared_ptr<OTCResponseData> otcResponseData,
                              OTCRequestResult result = OTCRequestResult::Rejected);

      QJsonObject toJson() const override;
      static std::shared_ptr<Response> fromJSON(const std::string& jsonData);
      void handle(ResponseHandler &) override;
      std::shared_ptr<OTCResponseData> otcResponseData() const;
      OTCRequestResult getResult() const;

   private:
      std::shared_ptr<OTCResponseData> otcResponseData_;
      OTCRequestResult result_;
   };
}

#endif // ANSWERCOMMONOTCRESPONSE_H
