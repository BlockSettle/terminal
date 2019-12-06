/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "OTCNegotiationResponseWidget.h"

#include "OtcTypes.h"
#include "UiUtils.h"
#include "Wallets/SyncWalletsManager.h"
#include "AuthAddressManager.h"
#include "OtcClient.h"
#include "AssetManager.h"
#include "ui_OTCNegotiationResponseWidget.h"

#include <QComboBox>
#include <QPushButton>

namespace {
   const QString paymentWallet = QObject::tr("Payment Wallet");
   const QString receivingWallet = QObject::tr("Receiving Wallet");
}

OTCNegotiationResponseWidget::OTCNegotiationResponseWidget(QWidget* parent)
   : OTCWindowsAdapterBase{ parent }
   , ui_{ new Ui::OTCNegotiationResponseWidget{} }
{
   ui_->setupUi(this);

   ui_->pushButtonCancel->setText(tr("Reject"));

   connect(ui_->offerSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged)
      , this, &OTCNegotiationResponseWidget::onChanged);
   connect(ui_->bidSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged)
      , this, &OTCNegotiationResponseWidget::onChanged);

   connect(ui_->comboBoxXBTWallets, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &OTCNegotiationResponseWidget::onCurrentWalletChanged);
   connect(ui_->pushButtonAccept, &QPushButton::clicked, this, &OTCNegotiationResponseWidget::onAcceptOrUpdateClicked);
   connect(ui_->pushButtonCancel, &QPushButton::clicked, this, &OTCNegotiationResponseWidget::responseRejected);
   connect(ui_->toolButtonXBTInputs, &QPushButton::clicked, this, &OTCNegotiationResponseWidget::onShowXBTInputsClicked);
   connect(this, &OTCWindowsAdapterBase::xbtInputsProcessed, this, &OTCNegotiationResponseWidget::onXbtInputsProcessed);

   ui_->quantitySpinBox->setEnabled(false);

   timeoutSec_ = getSeconds(bs::network::otc::negotiationTimeout());

   onChanged();
}

OTCNegotiationResponseWidget::~OTCNegotiationResponseWidget() = default;

void OTCNegotiationResponseWidget::setOffer(const bs::network::otc::Offer &offer)
{
   receivedOffer_ = offer;

   auto price = bs::network::otc::fromCents(offer.price);
   auto amount = bs::network::otc::satToBtc(offer.amount);
   const QString offerAndCurrency = QLatin1String("%1 %2");
   const bool isSell = offer.ourSide == bs::network::otc::Side::Sell;

   if (isSell) {
      ui_->recieveValue->setText(offerAndCurrency.arg(UiUtils::displayCurrencyAmount(price * amount)).arg(buyProduct_));
      ui_->deliverValue->setText(offerAndCurrency.arg(amount).arg(sellProduct_));
   }
   else {
      ui_->recieveValue->setText(offerAndCurrency.arg(amount).arg(sellProduct_));
      ui_->deliverValue->setText(offerAndCurrency.arg(UiUtils::displayCurrencyAmount(price * amount)).arg(buyProduct_));
   }

   const QString productToPrice = QLatin1String("%1 %2 / 1 %3");
   ui_->priceValue->setText(productToPrice.arg(price).arg(buyProduct_).arg(sellProduct_));

   ui_->quantitySpinBox->setValue(amount);
   ui_->quantitySpinBox->setDisabled(true);
   ui_->bidSpinBox->setValue(price);
   ui_->bidSpinBox->setEnabled(!isSell);
   ui_->offerSpinBox->setValue(price);
   ui_->offerSpinBox->setEnabled(isSell);
   ui_->receivingAddressWdgt->setVisible(!isSell);
   ui_->labelWallet->setText(isSell ? paymentWallet : receivingWallet);
   ui_->toolButtonXBTInputs->setVisible(offer.ourSide == bs::network::otc::Side::Sell);

   onChanged();
}

bs::network::otc::Offer OTCNegotiationResponseWidget::offer() const
{
   bs::network::otc::Offer result;
   result.ourSide = receivedOffer_.ourSide;
   result.amount = bs::network::otc::btcToSat(ui_->quantitySpinBox->value());
   if (receivedOffer_.ourSide == bs::network::otc::Side::Sell) {
      result.price = bs::network::otc::toCents(ui_->offerSpinBox->value());
   }
   else {
      result.price = bs::network::otc::toCents(ui_->bidSpinBox->value());
   }

   result.hdWalletId = ui_->comboBoxXBTWallets->currentData(UiUtils::WalletIdRole).toString().toStdString();
   result.authAddress = ui_->authenticationAddressComboBox->currentText().toStdString();

   if (ui_->receivingAddressComboBox->currentIndex() != 0) {
      result.recvAddress = ui_->receivingAddressComboBox->currentText().toStdString();
   }

   result.inputs = selectedUTXOs();

   return result;
}

void OTCNegotiationResponseWidget::setPeer(const bs::network::otc::Peer &peer)
{
   const bool isContact = (peer.type == bs::network::otc::PeerType::Contact);

   if (peer.type == bs::network::otc::PeerType::Request) {
      ui_->labelQuantityValue->setText(getXBTRange(peer.response.amount));
      ui_->labelBidValue->setText(getCCRange(peer.response.price));

      ui_->bidSpinBox->setMinimum(bs::network::otc::fromCents(peer.response.price.lower));
      ui_->bidSpinBox->setMaximum(bs::network::otc::fromCents(peer.response.price.upper));
      ui_->offerSpinBox->setMinimum(bs::network::otc::fromCents(peer.response.price.lower));
      ui_->offerSpinBox->setMaximum(bs::network::otc::fromCents(peer.response.price.upper));
   }

   ui_->rangeQuantity->setVisible(!isContact);
   ui_->rangeBid->setVisible(!isContact);

   setupTimer({ peer.stateTimestamp, ui_->progressBarTimeLeft, ui_->labelTimeLeft });
   setSelectedInputs(peer.offer.inputs);
}

void OTCNegotiationResponseWidget::onSyncInterface()
{
   int index = UiUtils::fillHDWalletsComboBox(ui_->comboBoxXBTWallets, getWalletManager(), UiUtils::WoWallets::Enable);
   ui_->comboBoxXBTWallets->setCurrentIndex(index);
   onCurrentWalletChanged();

   UiUtils::fillAuthAddressesComboBox(ui_->authenticationAddressComboBox, getAuthManager());
   ui_->widgetButtons->setEnabled(ui_->authenticationAddressComboBox->isEnabled());
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
   bool activateAcceptButton = true;
   double price = 0.0;
   if (receivedOffer_.ourSide == bs::network::otc::Side::Sell) {
      price = ui_->offerSpinBox->value();
   }
   else {
      price = ui_->bidSpinBox->value();
   }
   double quantity = ui_->quantitySpinBox->value();

   if (receivedOffer_.ourSide == bs::network::otc::Side::Sell
      && quantity > getXBTSpendableBalanceFromCombobox(ui_->comboBoxXBTWallets)) {
      activateAcceptButton = false;
   }
   else if (receivedOffer_.ourSide == bs::network::otc::Side::Buy
      && price * quantity
      > getAssetManager()->getBalance(buyProduct_.toStdString())) {
      activateAcceptButton = false;
   }

   if (receivedOffer_.ourSide == bs::network::otc::Side::Buy && !selectedUTXOs().empty()) {
      uint64_t totalSelected = 0;
      for (const auto &utxo : selectedUTXOs()) {
         totalSelected += utxo.getValue();
      }
      // This does not take into account pay-in fee
      if (totalSelected < static_cast<uint64_t>(receivedOffer_.amount)) {
         activateAcceptButton = false;
      }
   }

   ui_->pushButtonAccept->setEnabled(activateAcceptButton);

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

void OTCNegotiationResponseWidget::onShowXBTInputsClicked()
{
   ui_->toolButtonXBTInputs->setEnabled(false);
   showXBTInputsClicked(ui_->comboBoxXBTWallets);
}

void OTCNegotiationResponseWidget::onXbtInputsProcessed()
{
   onUpdateBalances();
   ui_->toolButtonXBTInputs->setEnabled(true);

   // Check selected amount and update accept button enabled state
   onChanged();
}

void OTCNegotiationResponseWidget::onCurrentWalletChanged()
{
   UiUtils::fillRecvAddressesComboBoxHDWallet(ui_->receivingAddressComboBox, getCurrentHDWalletFromCombobox(ui_->comboBoxXBTWallets));
   clearSelectedInputs();
   onUpdateBalances();
}
