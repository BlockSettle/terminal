#pragma once

#include "ListResponse.h"

namespace Chat {
   
   class ChatroomsListResponse : public ListResponse
   {
   public:
      ChatroomsListResponse(std::vector<std::string> dataList);
      ChatroomsListResponse(std::vector<std::shared_ptr<RoomData> > roomList);
      static std::shared_ptr<Response> fromJSON(const std::string& jsonData);
      void handle(ResponseHandler&) override;
      const std::vector<std::shared_ptr<RoomData>>& getChatRoomList() const;
   private:
      std::vector<std::shared_ptr<RoomData>> roomList_;
   };
   
}
