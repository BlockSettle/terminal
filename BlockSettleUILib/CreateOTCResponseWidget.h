#ifndef __CREATE_OTC_RESPONSE_WIDGET_H__
#define __CREATE_OTC_RESPONSE_WIDGET_H__

#include <QWidget>
#include <memory>

#include "ChatProtocol/DataObjects/OTCRequestData.h"
#include "CommonTypes.h"

namespace Ui {
   class CreateOTCResponseWidget;
};

class CreateOTCResponseWidget : public QWidget
{
Q_OBJECT

public:
   CreateOTCResponseWidget(QWidget* parent = nullptr);
   ~CreateOTCResponseWidget() override;

   void SetActiveOTCRequest(const std::shared_ptr<Chat::OTCRequestData>& otc);

   bs::network::OTCResponse GetCurrentOTCResponse() const;

private:
   void SetSide(const bs::network::ChatOTCSide::Type& side);
   void SetRange(const bs::network::OTCRangeID::Type& range);

   bs::network::OTCPriceRange    GetResponsePriceRange() const;
   bs::network::OTCQuantityRange GetResponseQuantityRange() const;

private slots:
   void OnCreateResponse();

signals:
   void ResponseCreated();

private:
   std::unique_ptr<Ui::CreateOTCResponseWidget> ui_;

   std::shared_ptr<Chat::OTCRequestData> currentOtcRequest_;
};

#endif // __CREATE_OTC_RESPONSE_WIDGET_H__
