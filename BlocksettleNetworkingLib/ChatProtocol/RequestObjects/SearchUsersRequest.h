#ifndef SEARCHUSERSREQUEST_H
#define SEARCHUSERSREQUEST_H

#include "Request.h"

namespace Chat {
    class SearchUsersRequest : public Request
    {
    public:
       SearchUsersRequest(const std::string clientId, const std::string senderId, const std::string& searchIdPattern);

    public:
       QJsonObject toJson() const override;
       static std::shared_ptr<Request> fromJSON(const std::string& clientId
                                      , const std::string& jsonData);
       void handle(RequestHandler &) override;
       std::string getSenderId() const;
       std::string getSearchIdPattern() const;

    private:
       std::string senderId_;
       std::string searchIdPattern_;
    };
}

#endif // SEARCHUSERSREQUEST_H
