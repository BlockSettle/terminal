/*

***********************************************************************************
* Copyright (C) 2020 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "FuturesTicket.h"
#include "ui_FuturesTicket.h"

#include "AssetManager.h"
#include "BSMessageBox.h"
#include "CommonTypes.h"
#include "CurrencyPair.h"
#include "QuoteProvider.h"
#include "UiUtils.h"
#include "XbtAmountValidator.h"

#include <QEvent>
#include <QKeyEvent>

#include <spdlog/spdlog.h>

namespace {
   const QString kEmptyInformationalLabelText = QStringLiteral("--");

   QString formatPrice(double price, bs::network::Asset::Type at) {
      if (price == 0) {
         return kEmptyInformationalLabelText;
      }
      return UiUtils::displayPriceForAssetType(price, at);
   }
}


FuturesTicket::FuturesTicket(QWidget* parent)
   : QWidget(parent)
   , ui_(new Ui::FuturesTicket())
{
   ui_->setupUi(this);

   invalidBalanceFont_ = ui_->labelBalanceValue->font();
   invalidBalanceFont_.setStrikeOut(true);

   xbtAmountValidator_ = new XbtAmountValidator(this);

   ui_->lineEditAmount->installEventFilter(this);

   connect(ui_->pushButtonSell, &QPushButton::clicked, this, &FuturesTicket::onSellSelected);
   connect(ui_->pushButtonBuy, &QPushButton::clicked, this, &FuturesTicket::onBuySelected);

   connect(ui_->pushButtonSubmit, &QPushButton::clicked, this, &FuturesTicket::submitButtonClicked);
   //connect(ui_->lineEditAmount, &QLineEdit::textEdited, this, &FuturesTicket::onAmountEdited);

   //disablePanel();
}

FuturesTicket::~FuturesTicket() = default;

void FuturesTicket::resetTicket()
{
   ui_->labelProductGroup->setText(kEmptyInformationalLabelText);
   ui_->labelSecurityId->setText(kEmptyInformationalLabelText);
   ui_->labelIndicativePrice->setText(kEmptyInformationalLabelText);

   currentBidPrice_ = kEmptyInformationalLabelText;
   currentOfferPrice_ = kEmptyInformationalLabelText;

   ui_->lineEditAmount->setValidator(nullptr);
   ui_->lineEditAmount->setEnabled(false);
   ui_->lineEditAmount->clear();
}

void FuturesTicket::init(const std::shared_ptr<spdlog::logger> &logger
   , const std::shared_ptr<AuthAddressManager> &authAddressManager
   , const std::shared_ptr<AssetManager>& assetManager
   , const std::shared_ptr<QuoteProvider> &quoteProvider)
{
   logger_ = logger;
   authAddressManager_ = authAddressManager;
   assetManager_ = assetManager;

   updateSubmitButton();
}

void FuturesTicket::setType(bs::network::Asset::Type type)
{
   type_ = type;
}

void FuturesTicket::SetCurrencyPair(const QString& currencyPair)
{
   ui_->labelSecurityId->setText(currencyPair);

   CurrencyPair cp(currencyPair.toStdString());

   currentProduct_ = QString::fromStdString(cp.NumCurrency());
   contraProduct_ = QString::fromStdString(cp.DenomCurrency());
   security_ = currencyPair;

   ui_->pushButtonNumCcy->setText(currentProduct_);
   ui_->pushButtonNumCcy->setChecked(true);

   ui_->pushButtonDenomCcy->setText(contraProduct_);
   ui_->pushButtonDenomCcy->setChecked(false);

   ui_->pushButtonDenomCcy->setEnabled(false);
}

void FuturesTicket::SetProductAndSide(const QString& productGroup, const QString& currencyPair
   , const QString& bidPrice, const QString& offerPrice, bs::network::Side::Type side)
{
   resetTicket();

   if (productGroup.isEmpty() || currencyPair.isEmpty()) {
      return;
   }

   //SetProductGroup(productGroup);
   SetCurrencyPair(currencyPair);
   //SetCurrentIndicativePrices(bidPrice, offerPrice);

  if (side == bs::network::Side::Type::Undefined) {
     side = bs::network::Side::Buy;
  }

  ui_->pushButtonSell->setChecked(side == bs::network::Side::Sell);
  ui_->pushButtonBuy->setChecked(side == bs::network::Side::Buy);

  productSelectionChanged();
}

void FuturesTicket::setSecurityId(const QString& productGroup, const QString& currencyPair
   , const QString& bidPrice, const QString& offerPrice)
{
   SetProductAndSide(productGroup, currencyPair, bidPrice, offerPrice, bs::network::Side::Undefined);
}

void FuturesTicket::setSecurityBuy(const QString& productGroup, const QString& currencyPair
   , const QString& bidPrice, const QString& offerPrice)
{
   SetProductAndSide(productGroup, currencyPair, bidPrice, offerPrice, bs::network::Side::Buy);
}

void FuturesTicket::setSecuritySell(const QString& productGroup, const QString& currencyPair
   , const QString& bidPrice, const QString& offerPrice)
{
   SetProductAndSide(productGroup, currencyPair, bidPrice, offerPrice, bs::network::Side::Sell);
}

bs::network::Side::Type FuturesTicket::getSelectedSide() const
{
   if (ui_->pushButtonSell->isChecked()) {
      return bs::network::Side::Sell;
   }

   return bs::network::Side::Buy;
}

QString FuturesTicket::getProduct() const
{
   return currentProduct_;
}

void FuturesTicket::productSelectionChanged()
{
   const auto &mdInfo = mdInfo_[type_][security_.toStdString()];

   const auto bidPrice = formatPrice(mdInfo.bidPrice, type_);
   const auto askPrice = formatPrice(mdInfo.askPrice, type_);

   ui_->labelBid->setText(bidPrice);
   ui_->labelAsk->setText(askPrice);
}

void FuturesTicket::updateSubmitButton()
{
}

bool FuturesTicket::eventFilter(QObject *watched, QEvent *evt)
{
   if (evt->type() == QEvent::KeyPress) {
      auto keyID = static_cast<QKeyEvent *>(evt)->key();
      if (ui_->pushButtonSubmit->isEnabled() && ((keyID == Qt::Key_Return) || (keyID == Qt::Key_Enter))) {
         submitButtonClicked();
      }
   }
   return QWidget::eventFilter(watched, evt);
}

void FuturesTicket::submitButtonClicked()
{
}

void FuturesTicket::onMDUpdate(bs::network::Asset::Type type, const QString &security, bs::network::MDFields mdFields)
{
   auto &mdInfo = mdInfo_[type][security.toStdString()];
   mdInfo.merge(bs::network::MDField::get(mdFields));

   if (type == type_) {
      productSelectionChanged();
   }
}

void FuturesTicket::onSellSelected()
{
   ui_->pushButtonSell->setChecked(true);
   ui_->pushButtonBuy->setChecked(false);
   productSelectionChanged();
}

void FuturesTicket::onBuySelected()
{
   ui_->pushButtonSell->setChecked(false);
   ui_->pushButtonBuy->setChecked(true);
   productSelectionChanged();
}
