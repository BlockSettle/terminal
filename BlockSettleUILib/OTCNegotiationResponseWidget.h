#ifndef __OTC_NEGOTIATION_RESPONSE_WIDGET_H__
#define __OTC_NEGOTIATION_RESPONSE_WIDGET_H__

#include <QWidget>
#include <memory>

#include "CommonTypes.h"
#include "chat.pb.h"

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

   void SetUpdateData(const std::shared_ptr<Chat::Data>& update
                      , const std::shared_ptr<Chat::Data>& initialResponse);

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

   bool changed_ = false;
};

#endif // __OTC_NEGOTIATION_RESPONSE_WIDGET_H__
