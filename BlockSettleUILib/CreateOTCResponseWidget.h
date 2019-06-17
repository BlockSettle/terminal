#ifndef __CREATE_OTC_RESPONSE_WIDGET_H__
#define __CREATE_OTC_RESPONSE_WIDGET_H__

#include <QWidget>
#include <memory>

#include "CommonTypes.h"
#include "chat.pb.h"

namespace Ui {
   class CreateOTCResponseWidget;
};

class CreateOTCResponseWidget : public QWidget
{
Q_OBJECT

public:
   CreateOTCResponseWidget(QWidget* parent = nullptr);
   ~CreateOTCResponseWidget() override;

   void SetRequestToRespond(const std::shared_ptr<Chat::Data>& otcRequest);
   void SetSubmittedResponse(const std::shared_ptr<Chat::Data>& otcResponse, const std::shared_ptr<Chat::Data>& otcRequest);

   bs::network::OTCResponse GetCurrentOTCResponse() const;

private:
   void InitUIFromRequest(const std::shared_ptr<Chat::Data>& otcRequest);

   void SetSide(const bs::network::ChatOTCSide::Type& side);
   void SetRange(const bs::network::OTCRangeID::Type& range);

   bs::network::OTCPriceRange    GetResponsePriceRange() const;
   bs::network::OTCQuantityRange GetResponseQuantityRange() const;

private slots:
   void OnCreateResponse();

signals:
   void ResponseCreated();
   void ResponseRejected();

private:
   std::unique_ptr<Ui::CreateOTCResponseWidget>    ui_;
   bs::network::ChatOTCSide::Type                  side_;
};

#endif // __CREATE_OTC_RESPONSE_WIDGET_H__
