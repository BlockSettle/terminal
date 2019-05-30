#ifndef __OTC_REQUEST_DATA_H__
#define __OTC_REQUEST_DATA_H__

#include "DataObject.h"
#include "ChatCommonTypes.h"

namespace Chat {

   class OTCRequestData : public DataObject
   {
   // DataObject interface
   public:
      QJsonObject toJson() const override;
      static std::shared_ptr<OTCRequestData> fromJSON(const std::string& jsonData);

   public:
      OTCRequestData(const QString& clientRequestId,
                     const QString& requestorId,
                     const QString& targetId,
                     const bs::network::OTCRequest& otcRequest);

      OTCRequestData(const QString& clientRequestId,
                     const QString& serverRequestId,
                     const QString& requestorId,
                     const QString& targetId,
                     uint64_t submitTimestamp,
                     uint64_t expireTimestamp,
                     const bs::network::OTCRequest& otcRequest);

      ~OTCRequestData() override = default;

      QString clientRequestId() const;
      QString serverRequestId() const;

      QString requestorId() const;
      // either user hash if DM, or OTC chat room name ( OTCRoomKey )
      QString targetId() const;

      uint64_t submitTimestamp() const;
      uint64_t expireTimestamp() const;

      const bs::network::OTCRequest& otcRequest() const;

   private:
      const QString clientRequestId_;
      const QString serverRequestId_;
      const QString requestorId_;
      const QString targetId_;

      const uint64_t submitTimestamp_;
      const uint64_t expireTimestamp_;

      const bs::network::OTCRequest otcRequest_;
   };

}

#endif // __OTC_REQUEST_DATA_H__
