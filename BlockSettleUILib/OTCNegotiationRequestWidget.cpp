#include "OTCNegotiationRequestWidget.h"

#include "ui_OTCNegotiationCommonWidget.h"

#include <QComboBox>
#include <QPushButton>

OTCNegotiationRequestWidget::OTCNegotiationRequestWidget(QWidget* parent)
   : QWidget{parent}
   , ui_{new Ui::OTCNegotiationCommonWidget{}}
{
   ui_->setupUi(this);

   ui_->headerLabel->setText(tr("OTC Request Negotiation"));

   ui_->spinBoxOffer->setAccelerated(true);
   ui_->spinBoxQuantity->setAccelerated(true);

   ui_->pushButtonCancel->setText(tr("Reject"));
   ui_->pushButtonAccept->setText(tr("Respond"));

   // changed
   connect(ui_->spinBoxOffer, QOverload<int>::of(&QSpinBox::valueChanged)
      , this, &OTCNegotiationRequestWidget::OnDataChanged);
   connect(ui_->spinBoxQuantity, QOverload<int>::of(&QSpinBox::valueChanged)
      , this, &OTCNegotiationRequestWidget::OnDataChanged);
   // push buttons
   connect(ui_->pushButtonCancel, &QPushButton::pressed, this, &OTCNegotiationRequestWidget::TradeRejected);
   connect(ui_->pushButtonAccept, &QPushButton::pressed, this, &OTCNegotiationRequestWidget::OnAcceptPressed);
}

OTCNegotiationRequestWidget::~OTCNegotiationRequestWidget() noexcept = default;

void OTCNegotiationRequestWidget::DisplayResponse(const std::shared_ptr<Chat::OTCResponseData>& initialResponse)
{
   if (initialResponse->otcResponse().side == bs::network::ChatOTCSide::Sell) {
      ui_->labelSide->setText(tr("Sell"));
   } else if (initialResponse->otcResponse().side == bs::network::ChatOTCSide::Buy) {
      ui_->labelSide->setText(tr("Buy"));
   } else {
      ui_->labelSide->setText(tr("Undefined"));
   }

   auto priceRange = initialResponse->otcResponse().priceRange;
   auto amountRange = initialResponse->otcResponse().quantityRange;

   ui_->spinBoxOffer->setMinimum(priceRange.lower);
   ui_->spinBoxOffer->setMaximum(priceRange.upper);
   ui_->spinBoxOffer->setValue(priceRange.lower);

   ui_->spinBoxQuantity->setMinimum(amountRange.lower);
   ui_->spinBoxQuantity->setMaximum(amountRange.upper);
   ui_->spinBoxQuantity->setValue(amountRange.lower);

   changed_ = false;
}

void OTCNegotiationRequestWidget::SetResponseData(const std::shared_ptr<Chat::OTCResponseData>& initialResponse)
{
   DisplayResponse(initialResponse);
   initialUpdate_ = true;
   ui_->pushButtonAccept->setText(tr("Respond"));
}

void OTCNegotiationRequestWidget::SetUpdateData(const std::shared_ptr<Chat::OTCUpdateData>& update
                                                , const std::shared_ptr<Chat::OTCResponseData>& initialResponse)
{
   DisplayResponse(initialResponse);
   initialUpdate_ = false;
   ui_->spinBoxOffer->setValue(update->otcUpdate().price);
   ui_->spinBoxQuantity->setValue(update->otcUpdate().amount);
   ui_->pushButtonAccept->setText(tr("Accept"));
   changed_ = false;
}

bs::network::OTCUpdate OTCNegotiationRequestWidget::GetUpdate() const
{
   bs::network::OTCUpdate update;

   update.amount = ui_->spinBoxQuantity->value();
   update.price = ui_->spinBoxOffer->value();

   return update;
}

void OTCNegotiationRequestWidget::OnDataChanged()
{
   if (!changed_ && !initialUpdate_) {
      changed_ = true;
      ui_->pushButtonAccept->setText(tr("Update"));
   }
}

void OTCNegotiationRequestWidget::OnAcceptPressed()
{
   if (changed_ || initialUpdate_) {
      emit TradeUpdated();
   } else {
      emit TradeAccepted();
   }
}
