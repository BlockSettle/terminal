#ifndef __OTC_CLOSE_TRADING_DATA_H__
#define __OTC_CLOSE_TRADING_DATA_H__

#include "DataObject.h"
#include "MessageData.h"
#include "ChatCommonTypes.h"

#include <string>

namespace Chat {

   class OTCCloseTradingData : public MessageData
   {
   public:
      OTCCloseTradingData(const QString &sender, const QString &receiver,
         const QString &id, const QDateTime &dateTime,
         int state = (int)State::Undefined);

      OTCCloseTradingData(const MessageData& source, const QJsonObject& jsonData);

      ~OTCCloseTradingData() override = default;

      QString displayText() const override;

      void messageDirectionUpdate() override;

   private:
      void updateDisplayString();

      static QString serializeCloseRequest();

   private:
      QString                    displayText_;
   };

}

#endif // __OTC_CLOSE_TRADING_DATA_H__
