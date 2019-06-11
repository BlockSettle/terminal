#ifndef __OTC_UPDATE_DATA_H__
#define __OTC_UPDATE_DATA_H__

#include "DataObject.h"
#include "MessageData.h"
#include "ChatCommonTypes.h"

#include <string>

namespace Chat {

   class OTCUpdateData : public MessageData
   {
   public:
      OTCUpdateData(const QString &sender, const QString &receiver,
         const QString &id, const QDateTime &dateTime,
         const bs::network::OTCUpdate& otcUpdate,
         int state = (int)State::Undefined);

      OTCUpdateData(const MessageData& source, const QJsonObject& jsonData);

      ~OTCUpdateData() override = default;

      QString displayText() const override;

      bool                       otcUpdateValid();
      bs::network::OTCUpdate     otcUpdate() const;
      void messageDirectionUpdate() override;

   private:
      static QString serializeUpdateData(const bs::network::OTCUpdate& update);

      void updateDisplayString();

   private:
      bool                       updateValid_ = false;;
      bs::network::OTCUpdate     otcUpdate_;
      QString                    displayText_;
   };

}

#endif // __OTC_UPDATE_DATA_H__
