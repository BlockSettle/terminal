#ifndef Request_h__
#define Request_h__

#include "../ProtocolDefinitions.h"
#include "../RequestHandler.h"
#include "../DataObjects.h"

namespace Chat {

   class Request : public Message<RequestType>
   {
   public:
   
      Request(RequestType requestType, const std::string& clientId);
   
      ~Request() override = default;
      static std::shared_ptr<Request> fromJSON(const std::string& clientId, const std::string& jsonData);
      virtual std::string getData() const;
      QJsonObject toJson() const override;
      virtual std::string getClientId() const;
      virtual void handle(RequestHandler &) = 0;
   
   protected:
      std::string    clientId_;
   };

}

#endif // Request_h__
