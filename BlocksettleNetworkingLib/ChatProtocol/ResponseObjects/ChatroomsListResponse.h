#pragma once

#include "ListResponse.h"

namespace Chat {
   
   class ChatroomsListResponse : public ListResponse
   {
   public:
      ChatroomsListResponse(std::vector<std::string> dataList);
      static std::shared_ptr<Response> fromJSON(const std::string& jsonData);
      void handle(ResponseHandler&) override;
   };
   
}
