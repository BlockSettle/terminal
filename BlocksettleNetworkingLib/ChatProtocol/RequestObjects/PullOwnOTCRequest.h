#ifndef PULLOWNOTCREQUEST_H
#define PULLOWNOTCREQUEST_H
#include "Request.h"

namespace Chat {
   class PullOwnOTCRequest : public Request
   {
   public:
      PullOwnOTCRequest(const std::string &clientId, const QString& targetId, const QString& serverOTCId);
      QJsonObject toJson() const override;
      static std::shared_ptr<Request> fromJSON(const std::string& clientId
                                               , const std::string& jsonData);
      void handle(RequestHandler&) override;


      QString targetId() const;

      QString serverOTCId() const;

   private:
      QString targetId_;
      QString serverOTCId_;
   };
}

#endif // PULLOWNOTCREQUEST_H
