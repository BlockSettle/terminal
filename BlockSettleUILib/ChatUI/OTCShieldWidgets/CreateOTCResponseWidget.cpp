#include "CreateOTCResponseWidget.h"
#include "UiUtils.h"
#include "Wallets/SyncWalletsManager.h"
#include "ui_CreateOTCResponseWidget.h"

using namespace bs::network;

CreateOTCResponseWidget::CreateOTCResponseWidget(QWidget* parent)
   : OTCWindowsAdapterBase{parent}
   , ui_{new Ui::CreateOTCResponseWidget{}}
{
   ui_->setupUi(this);

   connect(ui_->pushButtonSubmit, &QPushButton::clicked, this, &CreateOTCResponseWidget::responseCreated);
   connect(ui_->widgetAmountRange, &RangeWidget::upperValueChanged, this, &CreateOTCResponseWidget::updateAcceptButton);
   connect(ui_->widgetPriceRange, &RangeWidget::upperValueChanged, this, &CreateOTCResponseWidget::updateAcceptButton);
}

CreateOTCResponseWidget::~CreateOTCResponseWidget() = default;

void CreateOTCResponseWidget::setRequest(const otc::QuoteRequest &request)
{
   // TODO: Use MD
   ourSide_ = request.ourSide;

   double currentIndicativePrice = updateIndicativePriceValue(ui_->indicativePriceValue, ourSide_ != bs::network::otc::Side::Buy);
   int lowerBound = std::max(static_cast<int>(std::floor((currentIndicativePrice - 1000) / 1000) * 1000), 0);
   int upperBound = std::max(static_cast<int>(std::ceil((currentIndicativePrice + 1000) / 1000) * 1000), 1000);
   ui_->widgetPriceRange->SetRange(lowerBound, upperBound);

   ui_->sideValue->setText(QString::fromStdString(otc::toString(request.ourSide)));

   ui_->rangeValue->setText(QString::fromStdString(otc::toString(request.rangeType)));

   auto range = otc::getRange(request.rangeType);
   ui_->widgetAmountRange->SetRange(int(range.lower), int(range.upper));
   ui_->widgetAmountRange->SetLowerValue(int(range.lower));
   ui_->widgetAmountRange->SetUpperValue(int(range.upper));

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

void CreateOTCResponseWidget::onUpdateBalances()
{
   QString totalBalance = tr("%1 %2")
      .arg(UiUtils::displayAmount(getWalletManager()->getTotalBalance()))
      .arg(QString::fromStdString(bs::network::XbtCurrency));

   ui_->labelBalanceValue->setText(totalBalance);
}

void CreateOTCResponseWidget::onMDUpdated()
{
   updateIndicativePriceValue(ui_->indicativePriceValue, ourSide_ != bs::network::otc::Side::Buy);
}

void CreateOTCResponseWidget::updateAcceptButton()
{
   // We cannot offer zero as price
   const bool isEnabled = ui_->widgetAmountRange->GetUpperValue() != 0 && ui_->widgetPriceRange->GetUpperValue() != 0;
   ui_->pushButtonSubmit->setEnabled(isEnabled);
}
