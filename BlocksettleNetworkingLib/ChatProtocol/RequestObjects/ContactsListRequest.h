#ifndef ContactsListRequest_h__
#define ContactsListRequest_h__

#include "Request.h"
namespace Chat {

   class ContactsListRequest : public Request
   {
   public:
      ContactsListRequest(const std::string& clientId, const std::string& authId);

   public:
      QJsonObject toJson() const override;
      static std::shared_ptr<Request> fromJSON(const std::string& clientId
                                     , const std::string& jsonData);
      void handle(RequestHandler &) override;
      std::string getAuthId() const;
   private:
      std::string authId_;

   };

}

#endif // ContactsListRequest_h__
