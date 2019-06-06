#ifndef ANSWERCOMMONOTCREQUEST_H
#define ANSWERCOMMONOTCREQUEST_H

#include "Request.h"

namespace Chat {
   class AnswerCommonOTCRequest : public Request
   {
   public:
      AnswerCommonOTCRequest(const std::string &clientId,
                             std::shared_ptr<OTCResponseData> otcResponseData);

      QJsonObject toJson() const override;
      static std::shared_ptr<Request> fromJSON(const std::string& clientId
                                               , const std::string& jsonData);
      void handle(RequestHandler&) override;
      const std::shared_ptr<OTCResponseData> getOtcResponseData() const;
      std::string getResponsedId() const;
      std::string getRequestorId() const;

   private:
      std::shared_ptr<OTCResponseData> otcResponseData_;
      OTCResult result_;
   };
}

#endif // ANSWERCOMMONOTCREQUEST_H
