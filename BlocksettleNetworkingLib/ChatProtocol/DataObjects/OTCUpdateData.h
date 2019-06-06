#ifndef __OTC_UPDATE_DATA_H__
#define __OTC_UPDATE_DATA_H__

#include "DataObject.h"
#include "ChatCommonTypes.h"

namespace Chat {

   class OTCUpdateData : public DataObject
   {
   // DataObject interface
   public:
      QJsonObject toJson() const override;
      static std::shared_ptr<OTCUpdateData> fromJSON(const std::string& jsonData);

   public:
      OTCUpdateData() = delete;

      OTCUpdateData(const std::string& serverResponseId
                    , const std::string& clientUpdateId
                    , const std::string& updateSenderId
                    , const double amount
                    , const double price);

      OTCUpdateData(const std::string& serverResponseId
                    , const std::string& clientUpdateId
                    , const std::string& updateSenderId
                    , const std::string& updateReceiverId
                    , const uint64_t updateTimestamp
                    , const double amount
                    , const double price);

      ~OTCUpdateData() override = default;

      std::string serverResponseId() const;
      std::string clientUpdateId() const;

      std::string updateSenderId() const;

      std::string updateReceiverId() const;

      uint64_t updateTimestamp() const;

      double   amount() const;
      double   price() const;

      void     setUpdateReceiverId(const std::string& receiverId);
      void     setCurrentUpdateTimestamp();

   private:
      const std::string serverResponseId_;
      const std::string clientUpdateId_;
      const std::string updateSenderId_;
      std::string       updateReceiverId_;
      uint64_t          updateTimestamp_;
      const double      amount_;
      const double      price_;
   };
}

#endif // __OTC_UPDATE_DATA_H__
