#pragma once

#include "Response.h"

namespace Chat {
   
   class ContactsActionResponseServer : public PendingResponse
   {
   public:
      
      ContactsActionResponseServer(const std::string& userId, const std::string& contactId, ContactsActionServer requestedAction, ContactsActionServerResult actionResult, const std::string &message = std::string(""));
      QJsonObject toJson() const override;
      static std::shared_ptr<Response> fromJSON(const std::string& jsonData);
      void handle(ResponseHandler&) override;
      std::string userId() const;
      std::string contactId() const;
      std::string message() const;
      ContactsActionServer getRequestedAction() const;
      ContactsActionServerResult getActionResult() const;

   private:
      std::string userId_;
      std::string contactId_;
      std::string message_;
      ContactsActionServer requestedAction_;
      ContactsActionServerResult actionResult_;
      
   };
   
}
