#ifndef __OTC_RESPONSE_DATA_H__
#define __OTC_RESPONSE_DATA_H__

#include "DataObject.h"
#include "MessageData.h"
#include "ChatCommonTypes.h"

#include <string>

namespace Chat {

   class OTCResponseData : public MessageData
   {
   public:
      OTCResponseData(const QString &sender, const QString &receiver,
         const QString &id, const QDateTime &dateTime,
         const bs::network::OTCResponse& otcResponse,
         int state = (int)State::Undefined);

      OTCResponseData(const MessageData& source, const QJsonObject& jsonData);

      ~OTCResponseData() override = default;

      QString displayText() const override;

      bool                       otcResponseValid();
      bs::network::OTCResponse   otcResponse() const;
      void messageDirectionUpdate() override;

   private:
      static QString serializeResponseData(const bs::network::OTCResponse& otcResponse);

      void updateDisplayString();

   private:
      bool                       resopnseValid_ = false;;
      bs::network::OTCResponse   otcResponse_;
      QString                    displayText_;
   };

}

#endif // __OTC_RESPONSE_DATA_H__
