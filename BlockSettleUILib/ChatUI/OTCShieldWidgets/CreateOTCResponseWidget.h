#ifndef __CREATE_OTC_RESPONSE_WIDGET_H__
#define __CREATE_OTC_RESPONSE_WIDGET_H__

#include <memory>

#include "OTCWindowsAdapterBase.h"
#include "OtcTypes.h"

namespace Ui {
   class CreateOTCResponseWidget;
};

class CreateOTCResponseWidget : public OTCWindowsAdapterBase
{
   Q_OBJECT
public:
   CreateOTCResponseWidget(QWidget* parent = nullptr);
   ~CreateOTCResponseWidget() override;

   void setRequest(const bs::network::otc::QuoteRequest &request);

   bs::network::otc::QuoteResponse response() const;

signals:
   void responseCreated();

private:
   std::unique_ptr<Ui::CreateOTCResponseWidget> ui_;

   bs::network::otc::Side ourSide_{};

};

#endif // __CREATE_OTC_RESPONSE_WIDGET_H__
