#include "CreateOTCResponseWidget.h"

#include "ui_CreateOTCResponseWidget.h"

CreateOTCResponseWidget::CreateOTCResponseWidget(QWidget* parent)
   : QWidget{parent}
   , ui_{new Ui::CreateOTCResponseWidget{}}
{
   ui_->setupUi(this);

   ui_->widgetPriceRange->SetRange(3000, 4000);

   connect(ui_->pushButtonSubmit, &QPushButton::pressed, this, &CreateOTCResponseWidget::OnCreateResponse);
   connect(ui_->pushButtonPull, &QPushButton::pressed, this, &CreateOTCResponseWidget::ResponseRejected);
}

CreateOTCResponseWidget::~CreateOTCResponseWidget() = default;

void CreateOTCResponseWidget::SetRequestToRespond(const std::shared_ptr<Chat::OTCRequestData>& otc)
{
   ui_->pushButtonSubmit->setVisible(true);
   ui_->pushButtonPull->setText(tr("Reject"));

   InitUIFromRequest(otc);
}

void CreateOTCResponseWidget::InitUIFromRequest(const std::shared_ptr<Chat::OTCRequestData>& otcRequest)
{
   ui_->widgetPriceRange->setEnabled(true);

   SetSide(otcRequest->otcRequest().side);
   SetRange(otcRequest->otcRequest().amountRange);
}

void CreateOTCResponseWidget::SetSubmittedResponse(const std::shared_ptr<Chat::OTCResponseData>& otcResponse, const std::shared_ptr<Chat::OTCRequestData>& otcRequest)
{
   InitUIFromRequest(otcRequest);

   ui_->widgetPriceRange->SetLowerValue(otcResponse->otcResponse().priceRange.lower);
   ui_->widgetPriceRange->SetUpperValue(otcResponse->otcResponse().priceRange.upper);
   ui_->widgetPriceRange->setEnabled(false);

   ui_->widgetAmountRange->SetLowerValue(otcResponse->otcResponse().quantityRange.lower);
   ui_->widgetAmountRange->SetUpperValue(otcResponse->otcResponse().quantityRange.upper);
   ui_->widgetAmountRange->setEnabled(false);

   ui_->pushButtonSubmit->setVisible(false);
   ui_->pushButtonPull->setText(tr("Pull"));
}

void CreateOTCResponseWidget::OnCreateResponse()
{
   emit ResponseCreated();
}

void CreateOTCResponseWidget::SetSide(const bs::network::ChatOTCSide::Type& side)
{
   side_ = side;

   if (side == bs::network::ChatOTCSide::Sell) {
      ui_->labelSide->setText(tr("Sell"));
   } else if (side == bs::network::ChatOTCSide::Buy) {
      ui_->labelSide->setText(tr("Buy"));
   } else {
      ui_->labelSide->setText(tr("Undefined"));
   }
}

void CreateOTCResponseWidget::SetRange(const bs::network::OTCRangeID::Type& range)
{
   ui_->widgetAmountRange->setEnabled(true);

   ui_->labelRange->setText(QString::fromStdString(bs::network::OTCRangeID::toString(range)));

   switch (range) {
   case bs::network::OTCRangeID::Type::Range1_5:
      ui_->widgetAmountRange->SetRange(1, 5);
      break;
   case bs::network::OTCRangeID::Type::Range5_10:
      ui_->widgetAmountRange->SetRange(5, 10);
      break;
   case bs::network::OTCRangeID::Type::Range10_50:
      ui_->widgetAmountRange->SetRange(10, 50);
      break;
   case bs::network::OTCRangeID::Type::Range50_100:
      ui_->widgetAmountRange->SetRange(50, 100);
      break;
   case bs::network::OTCRangeID::Type::Range100_250:
      ui_->widgetAmountRange->SetRange(100, 250);
      break;
   case bs::network::OTCRangeID::Type::Range250plus:
   default:
      ui_->widgetAmountRange->SetRange(250, 1000);
      ui_->widgetAmountRange->setEnabled(false);
      break;
   }
}

bs::network::OTCPriceRange CreateOTCResponseWidget::GetResponsePriceRange() const
{
   bs::network::OTCPriceRange range;

   range.lower = ui_->widgetPriceRange->GetLowerValue();
   range.upper = ui_->widgetPriceRange->GetUpperValue();

   return range;
}

bs::network::OTCQuantityRange CreateOTCResponseWidget::GetResponseQuantityRange() const
{
   bs::network::OTCQuantityRange range;

   range.lower = ui_->widgetAmountRange->GetLowerValue();
   range.upper = ui_->widgetAmountRange->GetUpperValue();

   return range;
}


bs::network::OTCResponse CreateOTCResponseWidget::GetCurrentOTCResponse() const
{
   bs::network::OTCResponse response;

   response.side = side_;
   response.priceRange = GetResponsePriceRange();
   response.quantityRange = GetResponseQuantityRange();

   return response;
}
