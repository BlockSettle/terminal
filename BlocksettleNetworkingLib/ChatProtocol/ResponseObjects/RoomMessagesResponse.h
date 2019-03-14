#pragma once

#include "ListResponse.h"

namespace Chat {
   
   class RoomMessagesResponse : public ListResponse
   {
   public:
      RoomMessagesResponse(std::vector<std::string> dataList);
      static std::shared_ptr<Response> fromJSON(const std::string& jsonData);
      void handle(ResponseHandler &) override;
   };
}
