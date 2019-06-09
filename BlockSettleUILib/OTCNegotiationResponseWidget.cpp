#include "OTCNegotiationResponseWidget.h"

#include "ui_OTCNegotiationCommonWidget.h"

#include <QComboBox>
#include <QPushButton>

OTCNegotiationResponseWidget::OTCNegotiationResponseWidget(QWidget* parent)
   : QWidget{parent}
   , ui_{new Ui::OTCNegotiationCommonWidget{}}
{
   ui_->setupUi(this);

   ui_->headerLabel->setText(tr("OTC Response Negotiation"));

   ui_->pushButtonCancel->setText(tr("Pull"));
   ui_->pushButtonAccept->setText(tr("Accept"));

   // changed
   connect(ui_->spinBoxOffer, QOverload<int>::of(&QSpinBox::valueChanged)
      , this, &OTCNegotiationResponseWidget::OnDataChanged);
   connect(ui_->spinBoxQuantity, QOverload<int>::of(&QSpinBox::valueChanged)
      , this, &OTCNegotiationResponseWidget::OnDataChanged);
   // push buttons
   connect(ui_->pushButtonCancel, &QPushButton::pressed, this, &OTCNegotiationResponseWidget::TradeRejected);
   connect(ui_->pushButtonAccept, &QPushButton::pressed, this, &OTCNegotiationResponseWidget::OnAcceptPressed);
}

OTCNegotiationResponseWidget::~OTCNegotiationResponseWidget() noexcept = default;

void OTCNegotiationResponseWidget::DisplayResponse(const std::shared_ptr<Chat::OTCResponseData>& initialResponse)
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

void OTCNegotiationResponseWidget::SetUpdateData(const std::shared_ptr<Chat::OTCUpdateData>& update
                                                , const std::shared_ptr<Chat::OTCResponseData>& initialResponse)
{
   DisplayResponse(initialResponse);

   ui_->spinBoxOffer->setValue(update->otcUpdate().price);
   ui_->spinBoxQuantity->setValue(update->otcUpdate().amount);

   changed_ = false;
   ui_->pushButtonAccept->setText(tr("Accept"));
}

bs::network::OTCUpdate OTCNegotiationResponseWidget::GetUpdate() const
{
   bs::network::OTCUpdate update;

   update.amount = ui_->spinBoxQuantity->value();
   update.price = ui_->spinBoxOffer->value();

   return update;
}

void OTCNegotiationResponseWidget::OnDataChanged()
{
   if (!changed_) {
      changed_ = true;
      ui_->pushButtonAccept->setText(tr("Update"));
   }
}

void OTCNegotiationResponseWidget::OnAcceptPressed()
{
   if (changed_) {
      emit TradeUpdated();
   } else {
      emit TradeAccepted();
   }
}
