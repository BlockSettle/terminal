#ifndef __OTC_NEGOTIATION_REQUEST_WIDGET_H__
#define __OTC_NEGOTIATION_REQUEST_WIDGET_H__

#include <QWidget>
#include <memory>

#include "CommonTypes.h"

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

public:
   void DisplayResponse(const bs::network::Side::Type& side, const bs::network::OTCPriceRange& priceRange, const bs::network::OTCQuantityRange& amountRange);

private:
   std::unique_ptr<Ui::OTCNegotiationCommonWidget> ui_;
};

#endif // __OTC_NEGOTIATION_REQUEST_WIDGET_H__
