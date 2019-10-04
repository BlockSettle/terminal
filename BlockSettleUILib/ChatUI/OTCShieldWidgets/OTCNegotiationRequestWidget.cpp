#include "OTCNegotiationRequestWidget.h"

#include <QComboBox>
#include <QPushButton>

#include "AssetManager.h"
#include "AuthAddressManager.h"
#include "CoinControlDialog.h"
#include "OTCWindowsManager.h"
#include "OtcClient.h"
#include "OtcTypes.h"
#include "SelectedTransactionInputs.h"
#include "UiUtils.h"
#include "Wallets/SyncHDWallet.h"
#include "Wallets/SyncWalletsManager.h"
#include "ui_OTCNegotiationCommonWidget.h"

using namespace bs::network;

namespace {
   double kQuantityXBTSimpleStepAmount = 0.001;
}

OTCNegotiationRequestWidget::OTCNegotiationRequestWidget(QWidget* parent)
   : OTCWindowsAdapterBase{ parent }
   , ui_{ new Ui::OTCNegotiationCommonWidget{} }
{
   ui_->setupUi(this);

   ui_->headerLabel->setText(tr("OTC Request Negotiation"));

   ui_->priceSpinBoxRequest->setAccelerated(true);
   ui_->priceSpinBoxRequest->setAccelerated(true);

   ui_->pushButtonCancel->hide();
   ui_->pushButtonAccept->setText(tr("Submit"));

   connect(this, &OTCWindowsAdapterBase::chatRoomChanged, this, &OTCNegotiationRequestWidget::onChatRoomChanged);

   connect(ui_->pushButtonBuy, &QPushButton::clicked, this, &OTCNegotiationRequestWidget::onBuyClicked);
   connect(ui_->pushButtonBuy, &QPushButton::clicked, this, &OTCNegotiationRequestWidget::onUpdateBalances);
   connect(ui_->pushButtonSell, &QPushButton::clicked, this, &OTCNegotiationRequestWidget::onSellClicked);
   connect(ui_->pushButtonSell, &QPushButton::clicked, this, &OTCNegotiationRequestWidget::onUpdateBalances);
   connect(ui_->pushButtonAccept, &QPushButton::clicked, this, &OTCNegotiationRequestWidget::requestCreated);
   connect(ui_->toolButtonXBTInputs, &QPushButton::clicked, this, &OTCNegotiationRequestWidget::onShowXBTInputsClicked);
   connect(this, &OTCWindowsAdapterBase::xbtInputsProcessed, this, &OTCNegotiationRequestWidget::onXbtInputsProcessed);
   connect(ui_->pushButtonNumCcy, &QPushButton::clicked, this, &OTCNegotiationRequestWidget::onNumCcySelected);
   connect(ui_->comboBoxXBTWallets, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &OTCNegotiationRequestWidget::onCurrentWalletChanged);
   connect(ui_->priceUpdateButtonRequest, &QPushButton::clicked, this, &OTCNegotiationRequestWidget::onUpdateIndicativePrice);
   connect(ui_->quantityMaxButton, &QPushButton::clicked, this, &OTCNegotiationRequestWidget::onMaxQuantityClicked);

   connect(ui_->priceSpinBoxRequest, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &OTCNegotiationRequestWidget::onChanged);
   connect(ui_->quantitySpinBox, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &OTCNegotiationRequestWidget::onChanged);
   connect(ui_->authenticationAddressComboBox, qOverload<int>(&QComboBox::currentIndexChanged), this, &OTCNegotiationRequestWidget::onChanged);

   ui_->sideWidget->hide();
   ui_->priceLayoutResponse->hide();

   ui_->quantitySpinBox->setSingleStep(kQuantityXBTSimpleStepAmount);

   onSellClicked();
   onChanged();
}

OTCNegotiationRequestWidget::~OTCNegotiationRequestWidget() = default;

bs::network::otc::Offer OTCNegotiationRequestWidget::offer()
{
   bs::network::otc::Offer result;
   const bool isSell = ui_->pushButtonSell->isChecked();
   result.ourSide = isSell ? bs::network::otc::Side::Sell : bs::network::otc::Side::Buy;
   result.price = bs::network::otc::toCents(ui_->priceSpinBoxRequest->value());
   result.amount = bs::network::otc::btcToSat(ui_->quantitySpinBox->value());

   result.hdWalletId = ui_->comboBoxXBTWallets->currentData(UiUtils::WalletIdRole).toString().toStdString();
   result.authAddress = ui_->authenticationAddressComboBox->currentText().toStdString();

   if (!isSell && ui_->receivingAddressComboBox->currentIndex() != 0) {
      result.recvAddress = ui_->receivingAddressComboBox->currentText().toStdString();
   }

   result.inputs = selectedUTXO_;
   selectedUTXO_.clear();

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
         break;
      }

      case otc::PeerType::Request:
         toggleSideButtons(peer.request.ourSide == otc::Side::Sell);
         ui_->labelQuantityValue->setText(QString::fromStdString(otc::toString(peer.request.rangeType)));
         break;
      case otc::PeerType::Response: {
         // For public OTC side is fixed, use it from original request details
         toggleSideButtons(peer.response.ourSide == otc::Side::Sell);
         ui_->labelQuantityValue->setText(getXBTRange(peer.response.amount));
         ui_->labelBidValue->setText(getCCRange(peer.response.price));
         break;
      }
   }

   ui_->pushButtonBuy->setEnabled(isContact);
   ui_->pushButtonSell->setEnabled(isContact);
   ui_->rangeQuantity->setVisible(!isContact);
   ui_->rangeBid->setVisible(!isContact && peer.type == otc::PeerType::Response);

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

void OTCNegotiationRequestWidget::onMDUpdated()
{
   updateIndicativePriceValue(ui_->indicativePriceValue, ui_->pushButtonBuy->isChecked());
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
}

std::shared_ptr<bs::sync::hd::Wallet> OTCNegotiationRequestWidget::getCurrentHDWallet() const
{
   return getCurrentHDWalletFromCombobox(ui_->comboBoxXBTWallets);
}

BTCNumericTypes::balance_type OTCNegotiationRequestWidget::getXBTSpendableBalance() const
{
   return getXBTSpendableBalanceFromCombobox(ui_->comboBoxXBTWallets);
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
   ui_->indicativePriceValue->setText(UiUtils::displayPriceForAssetType(sellIndicativePrice_, productGroup_));
   ui_->quantitySpinBox->setValue(0.0);

   onUpdateIndicativePrice();
   selectedUTXO_.clear();
}

void OTCNegotiationRequestWidget::onBuyClicked()
{
   ui_->pushButtonSell->setChecked(false);
   ui_->pushButtonBuy->setChecked(true);
   ui_->toolButtonXBTInputs->setVisible(false);
   ui_->receivingAddressComboBox->setVisible(true);
   ui_->receivingAddressLabel->setVisible(true);
   ui_->quantityMaxButton->setVisible(false);
   ui_->indicativePriceValue->setText(UiUtils::displayPriceForAssetType(buyIndicativePrice_, productGroup_));
   ui_->quantitySpinBox->setValue(0.0);

   onUpdateIndicativePrice();
   selectedUTXO_.clear();
}

void OTCNegotiationRequestWidget::onShowXBTInputsClicked()
{
   ui_->toolButtonXBTInputs->setEnabled(false);
   showXBTInputsClicked(ui_->comboBoxXBTWallets);
}

void OTCNegotiationRequestWidget::onChanged()
{
   ui_->pushButtonAccept->setEnabled(ui_->priceSpinBoxRequest->value() > 0
      && ui_->quantitySpinBox->value() > 0
      && !ui_->authenticationAddressComboBox->currentText().isEmpty()
   );
}

void OTCNegotiationRequestWidget::onChatRoomChanged()
{
   selectedUTXO_.clear();
}

void OTCNegotiationRequestWidget::onCurrentWalletChanged()
{
   UiUtils::fillRecvAddressesComboBoxHDWallet(ui_->receivingAddressComboBox, getCurrentHDWallet());
   selectedUTXO_.clear();
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

void OTCNegotiationRequestWidget::onNumCcySelected()
{
   ui_->pushButtonNumCcy->setChecked(true);
   ui_->pushButtonDenomCcy->setChecked(false);
}

void OTCNegotiationRequestWidget::onUpdateIndicativePrice()
{
   const double indicativePrice = ui_->pushButtonBuy->isChecked() ? buyIndicativePrice_ : sellIndicativePrice_;
   ui_->priceSpinBoxRequest->setValue(indicativePrice);
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
               const uint64_t fee = OtcClient::estimatePayinFeeWithoutChange(utxos, feePerByte);
               const double spendableQuantity = std::max(0.0, (total - fee) / BTCNumericTypes::BalanceDivider);
               ui_->quantitySpinBox->setValue(spendableQuantity);
            });
         };
         otcManager_->getArmory()->estimateFee(OtcClient::feeTargetBlockCount(), feeCb);
      });
   };

   if (!selectedUTXO_.empty()) {
      cb(selectedUTXO_);
      return;
   }

   const auto &leaves = hdWallet->getGroup(hdWallet->getXBTGroupType())->getLeaves();
   std::vector<std::shared_ptr<bs::sync::Wallet>> wallets(leaves.begin(), leaves.end());
   bs::sync::Wallet::getSpendableTxOutList(wallets, cb);
}
