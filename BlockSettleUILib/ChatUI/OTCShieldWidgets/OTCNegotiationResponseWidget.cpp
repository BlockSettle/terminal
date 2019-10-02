#include "OTCNegotiationResponseWidget.h"

#include "OtcTypes.h"
#include "UiUtils.h"
#include "Wallets/SyncWalletsManager.h"
#include "AuthAddressManager.h"
#include "OtcClient.h"
#include "ui_OTCNegotiationCommonWidget.h"

#include <QComboBox>
#include <QPushButton>

OTCNegotiationResponseWidget::OTCNegotiationResponseWidget(QWidget* parent)
   : OTCWindowsAdapterBase{ parent }
   , ui_{ new Ui::OTCNegotiationCommonWidget{} }
{
   ui_->setupUi(this);

   ui_->headerLabel->setText(tr("OTC Response Negotiation"));

   ui_->pushButtonCancel->setText(tr("Reject"));

   connect(ui_->priceSpinBoxResponse, QOverload<double>::of(&QDoubleSpinBox::valueChanged)
      , this, &OTCNegotiationResponseWidget::onChanged);
   connect(ui_->quantitySpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged)
      , this, &OTCNegotiationResponseWidget::onChanged);

   connect(ui_->comboBoxXBTWallets, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &OTCNegotiationResponseWidget::onCurrentWalletChanged);
   connect(ui_->pushButtonAccept, &QPushButton::clicked, this, &OTCNegotiationResponseWidget::onAcceptOrUpdateClicked);
   connect(ui_->pushButtonCancel, &QPushButton::clicked, this, &OTCNegotiationResponseWidget::responseRejected);
   connect(ui_->priceUpdateButtonResponse, &QPushButton::clicked, this, &OTCNegotiationResponseWidget::onUpdateIndicativePrice);
   connect(ui_->toolButtonXBTInputs, &QPushButton::clicked, this, &OTCNegotiationResponseWidget::onShowXBTInputsClicked);
   connect(this, &OTCWindowsAdapterBase::xbtInputsProcessed, this, &OTCNegotiationResponseWidget::onXbtInputsProcessed);

   ui_->productSideWidget->hide();
   ui_->quantityMaxButton->hide();
   ui_->priceWidgetRequest->hide();
   ui_->quantitySpinBox->setEnabled(false);

   onChanged();
}

OTCNegotiationResponseWidget::~OTCNegotiationResponseWidget() = default;

void OTCNegotiationResponseWidget::setOffer(const bs::network::otc::Offer &offer)
{
   receivedOffer_ = offer;

   ui_->sideValue->setText(QString::fromStdString(bs::network::otc::toString(offer.ourSide)));
   ui_->priceSpinBoxResponse->setValue(bs::network::otc::fromCents(offer.price));
   ui_->quantitySpinBox->setValue(bs::network::otc::satToBtc(offer.amount));

   updateIndicativePriceValue();
   onChanged();
}

bs::network::otc::Offer OTCNegotiationResponseWidget::offer() const
{
   bs::network::otc::Offer result;
   result.ourSide = receivedOffer_.ourSide;
   result.price = bs::network::otc::toCents(ui_->priceSpinBoxResponse->value());
   result.amount = bs::network::otc::btcToSat(ui_->quantitySpinBox->value());

   result.hdWalletId = ui_->comboBoxXBTWallets->currentData(UiUtils::WalletIdRole).toString().toStdString();
   result.authAddress = ui_->authenticationAddressComboBox->currentText().toStdString();
   if (ui_->receivingAddressComboBox->currentIndex() != 0) {
      result.recvAddress = ui_->receivingAddressComboBox->currentText().toStdString();
   }
   return result;
}

void OTCNegotiationResponseWidget::setPeer(const bs::network::otc::Peer &peer)
{
   const bool isContact = (peer.type == bs::network::otc::PeerType::Contact);
   ui_->rangeWidget->setVisible(!isContact);
}

void OTCNegotiationResponseWidget::onAboutToApply()
{
   updateIndicativePriceValue();
}

void OTCNegotiationResponseWidget::onSyncInterface()
{
   int index = UiUtils::fillHDWalletsComboBox(ui_->comboBoxXBTWallets, getWalletManager());
   ui_->comboBoxXBTWallets->setCurrentIndex(index);
   onCurrentWalletChanged();

   UiUtils::fillAuthAddressesComboBox(ui_->authenticationAddressComboBox, getAuthManager());
}

void OTCNegotiationResponseWidget::onMDUpdated()
{
   updateIndicativePriceValue();
}

void OTCNegotiationResponseWidget::onUpdateBalances()
{
   double currentBalance = getXBTSpendableBalanceFromCombobox(ui_->comboBoxXBTWallets);
   QString totalBalance = tr("%1 %2")
      .arg(UiUtils::displayAmount(currentBalance))
      .arg(QString::fromStdString(bs::network::XbtCurrency));

   ui_->labelBalanceValue->setText(totalBalance);
}

void OTCNegotiationResponseWidget::onChanged()
{
   if (receivedOffer_ == offer()) {
      ui_->pushButtonAccept->setText(tr("Accept"));
   }
   else {
      ui_->pushButtonAccept->setText(tr("Update"));
   }
}

void OTCNegotiationResponseWidget::onAcceptOrUpdateClicked()
{
   if (receivedOffer_ == offer()) {
      emit responseAccepted();
   }
   else {
      emit responseUpdated();
   }
}

void OTCNegotiationResponseWidget::onUpdateIndicativePrice()
{
   const double indicativePrice = (receivedOffer_.ourSide == bs::network::otc::Side::Buy) ? buyIndicativePrice_ : sellIndicativePrice_;
   ui_->priceSpinBoxResponse->setValue(indicativePrice);
}

void OTCNegotiationResponseWidget::onShowXBTInputsClicked()
{
   ui_->toolButtonXBTInputs->setEnabled(false);
   showXBTInputsClicked(ui_->comboBoxXBTWallets);
}

void OTCNegotiationResponseWidget::onXbtInputsProcessed()
{
   onUpdateBalances();
   ui_->toolButtonXBTInputs->setEnabled(true);
}

void OTCNegotiationResponseWidget::onCurrentWalletChanged()
{
   UiUtils::fillRecvAddressesComboBoxHDWallet(ui_->receivingAddressComboBox, getCurrentHDWalletFromCombobox(ui_->comboBoxXBTWallets));
   selectedUTXO_.clear();
   onUpdateBalances();
}

void OTCNegotiationResponseWidget::updateIndicativePriceValue()
{
   OTCWindowsAdapterBase::updateIndicativePriceValue(ui_->indicativePriceValue, receivedOffer_.ourSide == bs::network::otc::Side::Buy);
}
