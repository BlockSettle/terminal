#ifndef __OTC_NEGOTIATION_RESPONSE_WIDGET_H__
#define __OTC_NEGOTIATION_RESPONSE_WIDGET_H__

#include <QWidget>
#include <memory>

#include "CommonTypes.h"

namespace Ui {
   class OTCNegotiationCommonWidget;
};


class OTCNegotiationResponseWidget : public QWidget
{
Q_OBJECT

public:
   OTCNegotiationResponseWidget(QWidget* parent = nullptr);
   ~OTCNegotiationResponseWidget() noexcept;

   OTCNegotiationResponseWidget(const OTCNegotiationResponseWidget&) = delete;
   OTCNegotiationResponseWidget& operator = (const OTCNegotiationResponseWidget&) = delete;

   OTCNegotiationResponseWidget(OTCNegotiationResponseWidget&&) = delete;
   OTCNegotiationResponseWidget& operator = (OTCNegotiationResponseWidget&&) = delete;

private:
   std::unique_ptr<Ui::OTCNegotiationCommonWidget> ui_;
};

#endif // __OTC_NEGOTIATION_RESPONSE_WIDGET_H__
