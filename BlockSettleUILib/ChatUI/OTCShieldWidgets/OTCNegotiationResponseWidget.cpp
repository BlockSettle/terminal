#include "OTCNegotiationResponseWidget.h"

#include "OtcTypes.h"
#include "UiUtils.h"
#include "Wallets/SyncWalletsManager.h"
#include "AuthAddressManager.h"
#include "ui_OTCNegotiationCommonWidget.h"

#include <QComboBox>
#include <QPushButton>

OTCNegotiationResponseWidget::OTCNegotiationResponseWidget(QWidget* parent)
   : OTCWindowsAdapterBase{parent}
   , ui_{new Ui::OTCNegotiationCommonWidget{}}
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
   connect(ui_->priceUpdateButtonRequest, &QPushButton::clicked, this, &OTCNegotiationResponseWidget::onUpdateIndicativePrice);

   ui_->productSideWidget->hide();
   ui_->indicativePriceWidget->hide();
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

void OTCNegotiationResponseWidget::onSyncInterface()
{
   int index = UiUtils::fillHDWalletsComboBox(ui_->comboBoxXBTWallets, getWalletManager());
   ui_->comboBoxXBTWallets->setCurrentIndex(index);
   onCurrentWalletChanged();

   UiUtils::fillAuthAddressesComboBox(ui_->authenticationAddressComboBox, getAuthManager());   
}

void OTCNegotiationResponseWidget::onUpdateMD(bs::network::Asset::Type type, const QString &security, const bs::network::MDFields& fields)
{
   if (productGroup_ != type || security_ != security) {
      return;
   }

   for (const auto &field : fields) {
      switch (field.type) {
      case bs::network::MDField::PriceBid:
         sellIndicativePrice_ = field.value;
         break;
      case bs::network::MDField::PriceOffer:
         buyIndicativePrice_ = field.value;
         break;
      default:  break;
      }
   }

   if (receivedOffer_.ourSide == bs::network::otc::Side::Buy) {
      ui_->indicativePriceValue->setText(UiUtils::displayPriceForAssetType(buyIndicativePrice_, productGroup_));
   }
   else if (receivedOffer_.ourSide == bs::network::otc::Side::Sell) {
      ui_->indicativePriceValue->setText(UiUtils::displayPriceForAssetType(sellIndicativePrice_, productGroup_));
   }
}

void OTCNegotiationResponseWidget::onUpdateBalances()
{
}

void OTCNegotiationResponseWidget::onChanged()
{
   if (receivedOffer_ == offer()) {
      ui_->pushButtonAccept->setText(tr("Accept"));
   } else {
      ui_->pushButtonAccept->setText(tr("Update"));
   }
}

void OTCNegotiationResponseWidget::onAcceptOrUpdateClicked()
{
   if (receivedOffer_ == offer()) {
      emit responseAccepted();
   } else {
      emit responseUpdated();
   }
}

void OTCNegotiationResponseWidget::onUpdateIndicativePrice()
{
   const double indicativePrice = (receivedOffer_.ourSide == bs::network::otc::Side::Buy) ? buyIndicativePrice_ : sellIndicativePrice_;
   ui_->priceSpinBoxRequest->setValue(indicativePrice);
}

void OTCNegotiationResponseWidget::onCurrentWalletChanged()
{
   const auto hdWalletId = ui_->comboBoxXBTWallets->currentData(UiUtils::WalletIdRole).toString().toStdString();
   UiUtils::fillRecvAddressesComboBoxHDWallet(ui_->receivingAddressComboBox, getWalletManager()->getHDWalletById(hdWalletId));
}
