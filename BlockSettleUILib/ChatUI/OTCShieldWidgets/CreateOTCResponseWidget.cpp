#include "CreateOTCResponseWidget.h"

#include "ui_CreateOTCResponseWidget.h"

using namespace bs::network;

CreateOTCResponseWidget::CreateOTCResponseWidget(QWidget* parent)
   : OTCWindowsAdapterBase{parent}
   , ui_{new Ui::CreateOTCResponseWidget{}}
{
   ui_->setupUi(this);

   connect(ui_->pushButtonSubmit, &QPushButton::clicked, this, &CreateOTCResponseWidget::responseCreated);
}

CreateOTCResponseWidget::~CreateOTCResponseWidget() = default;

void CreateOTCResponseWidget::setRequest(const otc::QuoteRequest &request)
{
   // TODO: Use MD
   ui_->widgetPriceRange->SetRange(8000, 10000);

   // TODO: Update balance

   ui_->labelSide->setText(QString::fromStdString(otc::toString(otc::switchSide(request.ourSide))));

   ui_->labelRange->setText(QString::fromStdString(otc::toString(request.rangeType)));

   auto range = otc::getRange(request.rangeType);
   ui_->widgetAmountRange->SetRange(int(range.lower), int(range.upper));
   ui_->widgetAmountRange->SetLowerValue(int(range.lower));
   ui_->widgetAmountRange->SetUpperValue(int(range.upper));

   ourSide_ = request.ourSide;

   ui_->pushButtonPull->hide();
}

otc::QuoteResponse CreateOTCResponseWidget::response() const
{
   otc::QuoteResponse response;
   response.ourSide = ourSide_;
   response.amount.lower = ui_->widgetAmountRange->GetLowerValue();
   response.amount.upper = ui_->widgetAmountRange->GetUpperValue();
   response.price.lower = otc::toCents(ui_->widgetPriceRange->GetLowerValue());
   response.price.upper = otc::toCents(ui_->widgetPriceRange->GetUpperValue());
   return response;
}
