#ifndef __OTC_REQUEST_DATA_H__
#define __OTC_REQUEST_DATA_H__

#include "DataObject.h"
#include "ChatCommonTypes.h"

#include <string>

namespace Chat {

   class OTCRequestData : public DataObject
   {
   // DataObject interface
   public:
      QJsonObject toJson() const override;
      static std::shared_ptr<OTCRequestData> fromJSON(const std::string& jsonData);

   public:
      OTCRequestData(const std::string& clientRequestId,
                     const std::string& requestorId,
                     const std::string& targetId,
                     const bs::network::OTCRequest& otcRequest);

      OTCRequestData(const std::string& clientRequestId,
                     const std::string& serverRequestId,
                     const std::string& requestorId,
                     const std::string& targetId,
                     uint64_t submitTimestamp,
                     uint64_t expireTimestamp,
                     const bs::network::OTCRequest& otcRequest);

      ~OTCRequestData() override = default;

      std::string clientRequestId() const;
      std::string serverRequestId() const;

      std::string requestorId() const;
      // either user hash if DM, or OTC chat room name ( OTCRoomKey )
      std::string targetId() const;

      uint64_t submitTimestamp() const;
      uint64_t expireTimestamp() const;

      const bs::network::OTCRequest& otcRequest() const;

      void setServerRequestId(const std::string &serverRequestId);
      void setExpireTimestamp(const uint64_t &expireTimestamp);

   private:
      const std::string clientRequestId_;
      std::string serverRequestId_;
      const std::string requestorId_;
      const std::string targetId_;

      const uint64_t submitTimestamp_;
      uint64_t expireTimestamp_;

      const bs::network::OTCRequest otcRequest_;
   };

}

#endif // __OTC_REQUEST_DATA_H__
