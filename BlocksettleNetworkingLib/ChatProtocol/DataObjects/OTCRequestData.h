#ifndef __OTC_REQUEST_DATA_H__
#define __OTC_REQUEST_DATA_H__

#include "DataObject.h"
#include "MessageData.h"
#include "ChatCommonTypes.h"

#include <string>

namespace Chat {

   class OTCRequestData : public MessageData
   {
   public:
      OTCRequestData(const QString &sender, const QString &receiver,
         const QString &id, const QDateTime &dateTime,
         const bs::network::OTCRequest& otcRequest,
         int state = (int)State::Undefined);

      OTCRequestData(const MessageData& source, const QJsonObject& jsonData);

      ~OTCRequestData() override = default;

      QString displayText() const override;

      bool                    otcReqeustValid();
      bs::network::OTCRequest otcRequest() const;
      void messageDirectionUpdate() override;

   private:
      static QString serializeRequestData(const bs::network::OTCRequest otcRequest);

      void updateDisplayString();

   private:
      bool                       requestValid_ = false;;
      bs::network::OTCRequest    otcRequest_;
      QString                    displayText_;
   };

}

#endif // __OTC_REQUEST_DATA_H__
