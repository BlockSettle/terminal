#ifndef ListResponse_h__
#define ListResponse_h__

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

#endif // ListResponse_h__
