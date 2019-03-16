#pragma once

#include "ListResponse.h"

namespace Chat {

   class UsersListResponse : public ListResponse
   {
   public:
      enum class Command {
         Replace = 0,
         Add,
         Delete
      };
      UsersListResponse(std::vector<std::string> dataList, Command cmd = Command::Replace);
      static std::shared_ptr<Response> fromJSON(const std::string& jsonData);
      QJsonObject toJson() const override;
      void handle(ResponseHandler &) override;
      Command command() const { return cmd_; }

   private:
      Command  cmd_;
   };

}
