#ifndef __OTC_NEGOTIATION_REQUEST_WIDGET_H__
#define __OTC_NEGOTIATION_REQUEST_WIDGET_H__

#include <QWidget>
#include <memory>

#include "CommonTypes.h"
#include "chat.pb.h"

namespace Ui {
   class OTCNegotiationCommonWidget;
};

class OTCNegotiationRequestWidget : public QWidget
{
Q_OBJECT

public:
   OTCNegotiationRequestWidget(QWidget* parent = nullptr);
   ~OTCNegotiationRequestWidget() noexcept;

   OTCNegotiationRequestWidget(const OTCNegotiationRequestWidget&) = delete;
   OTCNegotiationRequestWidget& operator = (const OTCNegotiationRequestWidget&) = delete;

   OTCNegotiationRequestWidget(OTCNegotiationRequestWidget&&) = delete;
   OTCNegotiationRequestWidget& operator = (OTCNegotiationRequestWidget&&) = delete;

   void SetUpdateData(const std::shared_ptr<Chat::Data>& update
                      , const std::shared_ptr<Chat::Data>& initialResponse);
   void SetResponseData(const std::shared_ptr<Chat::Data>& initialResponse);

   bs::network::OTCUpdate GetUpdate() const;

public slots:
   void OnDataChanged();
   void OnAcceptPressed();

signals:
   void TradeUpdated();
   void TradeAccepted();
   void TradeRejected();

public:
   void DisplayResponse(const std::shared_ptr<Chat::Data>& initialResponse);

private:
   std::unique_ptr<Ui::OTCNegotiationCommonWidget> ui_;
   bool initialUpdate_ = false;
   bool changed_ = false;
};

#endif // __OTC_NEGOTIATION_REQUEST_WIDGET_H__
