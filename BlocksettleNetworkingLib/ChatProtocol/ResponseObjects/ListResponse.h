#pragma once

#include "Response.h"

namespace Chat {
   class ListResponse : public Response
   {
   public:
      ListResponse(ResponseType responseType, std::vector<std::string> dataList);
      std::vector<std::string> getDataList() const { return dataList_; }
      static std::vector<std::string> fromJSON(const std::string& jsonData);
      QJsonObject toJson() const override;

   protected:
      std::vector<std::string> dataList_;
   };
}
