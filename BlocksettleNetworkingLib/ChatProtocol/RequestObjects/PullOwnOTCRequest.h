#ifndef PULLOWNOTCREQUEST_H
#define PULLOWNOTCREQUEST_H
#include "Request.h"

namespace Chat {
   class PullOwnOTCRequest : public Request
   {
   public:
      PullOwnOTCRequest(const std::string &clientId, const std::string& targetId, const std::string& serverOTCId);
      QJsonObject toJson() const override;
      static std::shared_ptr<Request> fromJSON(const std::string& clientId
                                               , const std::string& jsonData);
      void handle(RequestHandler&) override;


      std::string targetId() const;

      std::string serverOTCId() const;

   private:
      std::string targetId_;
      std::string serverOTCId_;
   };
}

#endif // PULLOWNOTCREQUEST_H
