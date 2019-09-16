#include "OTCNegotiationRequestWidget.h"

#include <QComboBox>
#include <QPushButton>

#include "OtcTypes.h"
#include "UiUtils.h"
#include "Wallets/SyncWalletsManager.h"
#include "Wallets/SyncHDWallet.h"
#include "AuthAddressManager.h"
#include "SelectedTransactionInputs.h"
#include "CoinControlDialog.h"
#include "ui_OTCNegotiationCommonWidget.h"

OTCNegotiationRequestWidget::OTCNegotiationRequestWidget(QWidget* parent)
   : OTCWindowsAdapterBase{ parent }
   , ui_{ new Ui::OTCNegotiationCommonWidget{} }
{
   ui_->setupUi(this);

   ui_->headerLabel->setText(tr("OTC Request Negotiation"));

   ui_->doubleSpinBoxOffer->setAccelerated(true);
   ui_->doubleSpinBoxQuantity->setAccelerated(true);

   ui_->pushButtonCancel->hide();
   ui_->pushButtonAccept->setText(tr("Submit"));

   connect(this, &OTCWindowsAdapterBase::chatRoomChanged, this, &OTCNegotiationRequestWidget::onChatRoomChanged);

   connect(ui_->pushButtonBuy, &QPushButton::clicked, this, &OTCNegotiationRequestWidget::onBuyClicked);
   connect(ui_->pushButtonSell, &QPushButton::clicked, this, &OTCNegotiationRequestWidget::onSellClicked);
   connect(ui_->pushButtonAccept, &QPushButton::clicked, this, &OTCNegotiationRequestWidget::requestCreated);
   connect(ui_->toolButtonXBTInputs, &QPushButton::clicked, this, &OTCNegotiationRequestWidget::onShowXBTInputsClicked);

   connect(ui_->comboBoxXBTWallets, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &OTCNegotiationRequestWidget::onCurrentWalletChanged);

   connect(ui_->doubleSpinBoxOffer, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &OTCNegotiationRequestWidget::onChanged);
   connect(ui_->doubleSpinBoxQuantity, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &OTCNegotiationRequestWidget::onChanged);

   ui_->widgetSideInfo->hide();

   onSellClicked();
   onChanged();
}

OTCNegotiationRequestWidget::~OTCNegotiationRequestWidget() = default;

bs::network::otc::Offer OTCNegotiationRequestWidget::offer()
{
   bs::network::otc::Offer result;
   const bool isSell = ui_->pushButtonSell->isChecked();
   result.ourSide = isSell ? bs::network::otc::Side::Sell : bs::network::otc::Side::Buy;
   result.price = bs::network::otc::toCents(ui_->doubleSpinBoxOffer->value());
   result.amount = bs::network::otc::btcToSat(ui_->doubleSpinBoxQuantity->value());

   result.hdWalletId = ui_->comboBoxXBTWallets->currentData(UiUtils::WalletIdRole).toString().toStdString();
   result.authAddress = ui_->authenticationAddressComboBox->currentText().toStdString();

   if (!isSell && ui_->receivingAddressComboBox->currentIndex() != 0) {
      result.recvAddress = ui_->receivingAddressComboBox->currentText().toStdString();
   }

   result.inputs = selectedUTXO;
   selectedUTXO.clear();

   return result;
}

void OTCNegotiationRequestWidget::syncInterface()
{
   int index = UiUtils::fillHDWalletsComboBox(ui_->comboBoxXBTWallets, getWalletManager());
   ui_->comboBoxXBTWallets->setCurrentIndex(index);
   onCurrentWalletChanged();

   UiUtils::fillAuthAddressesComboBox(ui_->authenticationAddressComboBox, getAuthManager());
}

std::shared_ptr<bs::sync::hd::Wallet> OTCNegotiationRequestWidget::getCurrentHDWallet() const
{
   const auto  walletId = ui_->comboBoxXBTWallets->currentData(UiUtils::WalletIdRole).toString().toStdString();
   return getWalletManager()->getHDWalletById(walletId);
}

void OTCNegotiationRequestWidget::onShowXBTInputReady()
{
   auto inputs = std::make_shared<SelectedTransactionInputs>(allUTXOs);
   CoinControlDialog dialog(inputs, false, this);
   int rc = dialog.exec();
   if (rc == QDialog::Accepted) {
      selectedUTXO = dialog.selectedInputs();
   }

   ui_->toolButtonXBTInputs->setEnabled(true);
}

void OTCNegotiationRequestWidget::onSellClicked()
{
   ui_->pushButtonSell->setChecked(true);
   ui_->pushButtonBuy->setChecked(false);
   ui_->toolButtonXBTInputs->setVisible(true);
   ui_->receivingAddressComboBox->setVisible(false);
   ui_->receivingAddressLabel->setVisible(false);

   selectedUTXO.clear();
}

void OTCNegotiationRequestWidget::onBuyClicked()
{
   ui_->pushButtonSell->setChecked(false);
   ui_->pushButtonBuy->setChecked(true);
   ui_->toolButtonXBTInputs->setVisible(false);
   ui_->receivingAddressComboBox->setVisible(true);
   ui_->receivingAddressLabel->setVisible(true);

   selectedUTXO.clear();
}

void OTCNegotiationRequestWidget::onShowXBTInputsClicked()
{
   ui_->toolButtonXBTInputs->setEnabled(false);
   allUTXOs.clear();
   awaitingLeafsResponse.clear();
   selectedUTXO.clear();

   const auto hdWallet = getCurrentHDWallet();
   for (auto wallet : hdWallet->getGroup(hdWallet->getXBTGroupType())->getLeaves()) {
      auto cbUTXOs = [parentWidget = QPointer<OTCNegotiationRequestWidget>(this), walletId = wallet->walletId()](const std::vector<UTXO> &utxos) {
         if (!parentWidget) {
            return;
         }

         parentWidget->allUTXOs.insert(parentWidget->allUTXOs.end(), utxos.begin(), utxos.end());
         parentWidget->awaitingLeafsResponse.erase(walletId);

         if (parentWidget->awaitingLeafsResponse.empty()) {
            QMetaObject::invokeMethod(parentWidget, "onShowXBTInputReady");
         }
      };

      if (!wallet->getSpendableTxOutList(cbUTXOs, UINT64_MAX)) {
         continue;
      }

      awaitingLeafsResponse.insert(wallet->walletId());
   }
}

void OTCNegotiationRequestWidget::onChanged()
{
   ui_->pushButtonAccept->setEnabled(ui_->doubleSpinBoxOffer->value() > 0 && ui_->doubleSpinBoxQuantity->value() > 0);
}

void OTCNegotiationRequestWidget::onChatRoomChanged()
{
   selectedUTXO.clear();
}

void OTCNegotiationRequestWidget::onCurrentWalletChanged()
{
   UiUtils::fillRecvAddressesComboBoxHDWallet(ui_->receivingAddressComboBox, getCurrentHDWallet());
}
