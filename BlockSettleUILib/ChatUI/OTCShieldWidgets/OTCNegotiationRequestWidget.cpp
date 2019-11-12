#include "OTCNegotiationRequestWidget.h"

#include "AssetManager.h"
#include "AuthAddressManager.h"
#include "CoinControlDialog.h"
#include "OTCWindowsManager.h"
#include "OtcTypes.h"
#include "SelectedTransactionInputs.h"
#include "TradesUtils.h"
#include "UiUtils.h"
#include "Wallets/SyncHDWallet.h"
#include "Wallets/SyncWalletsManager.h"
#include "ui_OTCNegotiationRequestWidget.h"

#include <QComboBox>
#include <QPushButton>
#include <QKeyEvent>

using namespace bs::network;

namespace {
   double kQuantityXBTSimpleStepAmount = 0.001;
   const QString paymentWallet = QObject::tr("Payment Wallet");
   const QString receivingWallet = QObject::tr("Receiving Wallet");
}

OTCNegotiationRequestWidget::OTCNegotiationRequestWidget(QWidget* parent)
   : OTCWindowsAdapterBase{ parent }
   , ui_{ new Ui::OTCNegotiationRequestWidget{} }
{
   ui_->setupUi(this);

   ui_->priceSpinBox->setAccelerated(true);
   ui_->priceSpinBox->setAccelerated(true);

   connect(ui_->pushButtonBuy, &QPushButton::clicked, this, &OTCNegotiationRequestWidget::onBuyClicked);
   connect(ui_->pushButtonBuy, &QPushButton::clicked, this, &OTCNegotiationRequestWidget::onUpdateBalances);
   connect(ui_->pushButtonSell, &QPushButton::clicked, this, &OTCNegotiationRequestWidget::onSellClicked);
   connect(ui_->pushButtonSell, &QPushButton::clicked, this, &OTCNegotiationRequestWidget::onUpdateBalances);
   connect(ui_->pushButtonAcceptRequest, &QPushButton::clicked, this, &OTCNegotiationRequestWidget::requestCreated);
   connect(ui_->toolButtonXBTInputs, &QPushButton::clicked, this, &OTCNegotiationRequestWidget::onShowXBTInputsClicked);
   connect(this, &OTCWindowsAdapterBase::xbtInputsProcessed, this, &OTCNegotiationRequestWidget::onXbtInputsProcessed);
   connect(ui_->comboBoxXBTWallets, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &OTCNegotiationRequestWidget::onCurrentWalletChanged);
   connect(ui_->quantityMaxButton, &QPushButton::clicked, this, &OTCNegotiationRequestWidget::onMaxQuantityClicked);

   connect(ui_->priceSpinBox, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &OTCNegotiationRequestWidget::onChanged);
   connect(ui_->quantitySpinBox, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &OTCNegotiationRequestWidget::onChanged);
   connect(ui_->authenticationAddressComboBox, qOverload<int>(&QComboBox::currentIndexChanged), this, &OTCNegotiationRequestWidget::onChanged);

   ui_->quantitySpinBox->setSingleStep(kQuantityXBTSimpleStepAmount);

   onSellClicked();
   onChanged();
}

OTCNegotiationRequestWidget::~OTCNegotiationRequestWidget() = default;

bs::network::otc::Offer OTCNegotiationRequestWidget::offer() const
{
   bs::network::otc::Offer result;
   const bool isSell = ui_->pushButtonSell->isChecked();
   result.ourSide = isSell ? bs::network::otc::Side::Sell : bs::network::otc::Side::Buy;
   result.price = bs::network::otc::toCents(ui_->priceSpinBox->value());
   result.amount = bs::network::otc::btcToSat(ui_->quantitySpinBox->value());

   result.hdWalletId = ui_->comboBoxXBTWallets->currentData(UiUtils::WalletIdRole).toString().toStdString();
   result.authAddress = ui_->authenticationAddressComboBox->currentText().toStdString();

   if (!isSell && ui_->receivingAddressComboBox->currentIndex() != 0) {
      result.recvAddress = ui_->receivingAddressComboBox->currentText().toStdString();
   }

   result.inputs = selectedUTXOs();

   return result;
}

void OTCNegotiationRequestWidget::onAboutToApply()
{
   onUpdateIndicativePrice();
}

void OTCNegotiationRequestWidget::setPeer(const bs::network::otc::Peer &peer)
{
   const bool isContact = (peer.type == otc::PeerType::Contact);

   switch (peer.type) {
      case otc::PeerType::Contact: {
         // Reset side to sell by default for contacts
         toggleSideButtons(/*isSell*/ true);
         ui_->quantitySpinBox->setMinimum(0);
         ui_->priceSpinBox->setMinimum(0);
         ui_->quantitySpinBox->setMaximum(std::numeric_limits<double>::max());
         ui_->priceSpinBox->setMaximum(std::numeric_limits<double>::max());
         break;
      }
      case otc::PeerType::Request: {
            toggleSideButtons(peer.request.ourSide == otc::Side::Sell);
            ui_->labelQuantityValue->setText(QString::fromStdString(otc::toString(peer.request.rangeType)));
            const auto range = otc::getRange(peer.request.rangeType);
            ui_->quantitySpinBox->setMinimum(range.lower);
            ui_->quantitySpinBox->setMaximum(range.upper);
            break;
      }
      case otc::PeerType::Response: {
         // For public OTC side is fixed, use it from original request details
         toggleSideButtons(peer.response.ourSide == otc::Side::Sell);
         ui_->labelQuantityValue->setText(getXBTRange(peer.response.amount));
         ui_->labelBidValue->setText(getCCRange(peer.response.price));
         ui_->quantitySpinBox->setMinimum(peer.response.amount.lower);
         ui_->quantitySpinBox->setMaximum(peer.response.amount.upper);
         ui_->priceSpinBox->setMinimum(bs::network::otc::fromCents(peer.response.price.lower));
         ui_->priceSpinBox->setMaximum(bs::network::otc::fromCents(peer.response.price.upper));
         break;
      }
   }

   ui_->pushButtonBuy->setEnabled(isContact);
   ui_->pushButtonSell->setEnabled(isContact);
   ui_->rangeQuantity->setVisible(!isContact);
   ui_->rangeBid->setVisible(!isContact && peer.type == otc::PeerType::Response);

   setSelectedInputs(peer.offer.inputs);
   onChanged();
}

void OTCNegotiationRequestWidget::onSyncInterface()
{
   int index = UiUtils::fillHDWalletsComboBox(ui_->comboBoxXBTWallets, getWalletManager());
   ui_->comboBoxXBTWallets->setCurrentIndex(index);
   onCurrentWalletChanged();

   UiUtils::fillAuthAddressesComboBox(ui_->authenticationAddressComboBox, getAuthManager());
   ui_->widgetButtons->setEnabled(ui_->authenticationAddressComboBox->isEnabled());
}

void OTCNegotiationRequestWidget::onUpdateBalances()
{
   QString totalBalance;
   // #new_logic : fix me when different products security will be available
   if (ui_->pushButtonBuy->isChecked()) {
      totalBalance = tr("%1 %2")
         .arg(UiUtils::displayCurrencyAmount(getAssetManager()->getBalance(buyProduct_.toStdString())))
         .arg(buyProduct_);
      ui_->quantitySpinBox->setMaximum(std::numeric_limits<double>::max());
   }
   else {
      double currentBalance = getXBTSpendableBalance();
      totalBalance = tr("%1 %2")
         .arg(UiUtils::displayAmount(currentBalance))
         .arg(QString::fromStdString(bs::network::XbtCurrency));
      ui_->quantitySpinBox->setMaximum(currentBalance);
   }

   ui_->labelBalanceValue->setText(totalBalance);
   onChanged();
}

std::shared_ptr<bs::sync::hd::Wallet> OTCNegotiationRequestWidget::getCurrentHDWallet() const
{
   return getCurrentHDWalletFromCombobox(ui_->comboBoxXBTWallets);
}

BTCNumericTypes::balance_type OTCNegotiationRequestWidget::getXBTSpendableBalance() const
{
   return getXBTSpendableBalanceFromCombobox(ui_->comboBoxXBTWallets);
}

void OTCNegotiationRequestWidget::keyPressEvent(QKeyEvent* event)
{
   OTCWindowsAdapterBase::keyPressEvent(event);
   if ((event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return)
      && ui_->pushButtonAcceptRequest->isEnabled()) {
      emit requestCreated();
   }
}

void OTCNegotiationRequestWidget::onXbtInputsProcessed()
{
   onUpdateBalances();
   ui_->toolButtonXBTInputs->setEnabled(true);
}

void OTCNegotiationRequestWidget::onSellClicked()
{
   ui_->pushButtonSell->setChecked(true);
   ui_->pushButtonBuy->setChecked(false);
   ui_->toolButtonXBTInputs->setVisible(true);
   ui_->receivingAddressComboBox->setVisible(false);
   ui_->receivingAddressLabel->setVisible(false);
   ui_->quantityMaxButton->setVisible(true);
   ui_->quantitySpinBox->setValue(0.0);
   ui_->labelWallet->setText(paymentWallet);

   onUpdateIndicativePrice();
}

void OTCNegotiationRequestWidget::onBuyClicked()
{
   ui_->pushButtonSell->setChecked(false);
   ui_->pushButtonBuy->setChecked(true);
   ui_->toolButtonXBTInputs->setVisible(false);
   ui_->receivingAddressComboBox->setVisible(true);
   ui_->receivingAddressLabel->setVisible(true);
   ui_->quantityMaxButton->setVisible(false);
   ui_->quantitySpinBox->setValue(0.0);
   ui_->labelWallet->setText(receivingWallet);

   onUpdateIndicativePrice();
}

void OTCNegotiationRequestWidget::onShowXBTInputsClicked()
{
   ui_->toolButtonXBTInputs->setEnabled(false);
   showXBTInputsClicked(ui_->comboBoxXBTWallets);
}

void OTCNegotiationRequestWidget::onChanged()
{
   bool activateAcceptButton = ui_->priceSpinBox->value() > 0
      && ui_->quantitySpinBox->value() > 0
      && !ui_->authenticationAddressComboBox->currentText().isEmpty();

   if (!activateAcceptButton) {
      ui_->pushButtonAcceptRequest->setDisabled(true);
      return;
   }

   if (ui_->pushButtonBuy->isChecked()) {
      ui_->quantitySpinBox->setMaximum(
         getAssetManager()->getBalance(buyProduct_.toStdString()) / ui_->priceSpinBox->value());
   }

   ui_->pushButtonAcceptRequest->setEnabled(true);
}

void OTCNegotiationRequestWidget::onChatRoomChanged()
{
   clearSelectedInputs();
}

void OTCNegotiationRequestWidget::onCurrentWalletChanged()
{
   UiUtils::fillRecvAddressesComboBoxHDWallet(ui_->receivingAddressComboBox, getCurrentHDWallet());
   clearSelectedInputs();
   onUpdateBalances();
}

void OTCNegotiationRequestWidget::toggleSideButtons(bool isSell)
{
   ui_->pushButtonSell->setChecked(isSell);
   ui_->pushButtonBuy->setChecked(!isSell);
   if (isSell) {
      onSellClicked();
   }
   else {
      onBuyClicked();
   }
}

void OTCNegotiationRequestWidget::onUpdateIndicativePrice()
{
   const double indicativePrice = ui_->pushButtonBuy->isChecked() ? buyIndicativePrice_ : sellIndicativePrice_;
   ui_->priceSpinBox->setValue(indicativePrice);
}

void OTCNegotiationRequestWidget::onMaxQuantityClicked()
{
   const auto hdWallet = getCurrentHDWalletFromCombobox(ui_->comboBoxXBTWallets);
   if (!hdWallet) {
      ui_->quantitySpinBox->setValue(0);
      return;
   }

   auto cb = [this, parentWidget = QPointer<OTCWindowsAdapterBase>(this)](const std::vector<UTXO> &utxos) {
      QMetaObject::invokeMethod(qApp, [this, utxos, parentWidget] {
         if (!parentWidget) {
            return;
         }
         auto feeCb = [this, parentWidget, utxos = std::move(utxos)](float fee) {
            QMetaObject::invokeMethod(qApp, [this, parentWidget, fee, utxos = std::move(utxos)] {
               if (!parentWidget) {
                  return;
               }
               float feePerByte = ArmoryConnection::toFeePerByte(fee);
               uint64_t total = 0;
               for (const auto &utxo : utxos) {
                  total += utxo.getValue();
               }
               const uint64_t fee = bs::tradeutils::estimatePayinFeeWithoutChange(utxos, feePerByte);
               const double spendableQuantity = std::max(0.0, (total - fee) / BTCNumericTypes::BalanceDivider);
               ui_->quantitySpinBox->setValue(spendableQuantity);
            });
         };
         otcManager_->getArmory()->estimateFee(bs::tradeutils::feeTargetBlockCount(), feeCb);
      });
   };

   if (!selectedUTXOs().empty()) {
      cb(selectedUTXOs());
      return;
   }

   const auto &leaves = hdWallet->getGroup(hdWallet->getXBTGroupType())->getLeaves();
   std::vector<std::shared_ptr<bs::sync::Wallet>> wallets(leaves.begin(), leaves.end());
   bs::sync::Wallet::getSpendableTxOutList(wallets, cb);
}
