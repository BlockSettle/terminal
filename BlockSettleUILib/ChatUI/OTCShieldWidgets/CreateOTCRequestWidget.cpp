/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "CreateOTCRequestWidget.h"

#include "OtcTypes.h"
#include "Wallets/SyncWalletsManager.h"
#include "UiUtils.h"
#include "AssetManager.h"
#include "ui_CreateOTCRequestWidget.h"

#include <QComboBox>
#include <QPushButton>

using namespace bs::network;

CreateOTCRequestWidget::CreateOTCRequestWidget(QWidget* parent)
   : OTCWindowsAdapterBase{parent}
   , ui_{new Ui::CreateOTCRequestWidget{}}
{
   ui_->setupUi(this);
}

CreateOTCRequestWidget::~CreateOTCRequestWidget() = default;

void CreateOTCRequestWidget::init(bs::network::otc::Env env)
{
   env_ = static_cast<int>(env);
   connect(ui_->pushButtonBuy, &QPushButton::clicked, this, &CreateOTCRequestWidget::onBuyClicked);
   connect(ui_->pushButtonSell, &QPushButton::clicked, this, &CreateOTCRequestWidget::onSellClicked);
   connect(ui_->pushButtonSubmit, &QPushButton::clicked, this, &CreateOTCRequestWidget::requestCreated);
   connect(ui_->pushButtonNumCcy, &QPushButton::clicked, this, &CreateOTCRequestWidget::onNumCcySelected);

   onSellClicked();
}

otc::QuoteRequest CreateOTCRequestWidget::request() const
{
   bs::network::otc::QuoteRequest result;
   result.rangeType = otc::RangeType(ui_->comboBoxRange->currentData().toInt());
   result.ourSide = ui_->pushButtonSell->isChecked() ? otc::Side::Sell : otc::Side::Buy;
   return result;
}

void CreateOTCRequestWidget::onSellClicked()
{
   ui_->pushButtonSell->setChecked(true);
   ui_->pushButtonBuy->setChecked(false);
   onUpdateBalances();
}

void CreateOTCRequestWidget::onBuyClicked()
{
   ui_->pushButtonSell->setChecked(false);
   ui_->pushButtonBuy->setChecked(true);

   onUpdateBalances();
}

void CreateOTCRequestWidget::onNumCcySelected()
{
   ui_->pushButtonNumCcy->setChecked(true);
   ui_->pushButtonDenomCcy->setChecked(false);
}

void CreateOTCRequestWidget::onUpdateBalances()
{
   QString totalBalance;
   if (ui_->pushButtonBuy->isChecked()) {
      const auto totalAssetBalance = getAssetManager()->getBalance(buyProduct_.toStdString());

      totalBalance = tr("%1 %2")
         .arg(UiUtils::displayCurrencyAmount(totalAssetBalance))
         .arg(buyProduct_);
      updateXBTRange(false, totalAssetBalance);
   }
   else {
      const auto totalXBTBalance = getWalletManager()->getTotalBalance();

      totalBalance = tr("%1 %2")
         .arg(UiUtils::displayAmount(totalXBTBalance))
         .arg(QString::fromStdString(bs::network::XbtCurrency));

      updateXBTRange(true, totalXBTBalance);
   }

   ui_->labelBalanceValue->setText(totalBalance);
}

void CreateOTCRequestWidget::updateXBTRange(bool isSell, BTCNumericTypes::balance_type balance /*= 0.0*/)
{
   otc::RangeType selectedRangeType = static_cast<otc::RangeType>(ui_->comboBoxRange->currentData().toInt());

   ui_->comboBoxRange->clear();

   auto env = static_cast<bs::network::otc::Env>(env_);
   auto lowestRangeType = otc::firstRangeValue(env);
   ui_->comboBoxRange->addItem(QString::fromStdString(otc::toString(lowestRangeType)), static_cast<int>(lowestRangeType));

   otc::Range lowestRange = otc::getRange(lowestRangeType);

   auto checkLowestPossible = [&](int64_t lowestPrice) -> bool {
      return (isSell && lowestPrice > balance) || (!isSell && lowestPrice > balance / buyIndicativePrice_);
   };

   if (checkLowestPossible(lowestRange.lower)) {
      ui_->comboBoxRange->setDisabled(true);
      ui_->pushButtonSubmit->setDisabled(true);
      return;
   }

   ui_->comboBoxRange->setEnabled(true);
   ui_->pushButtonSubmit->setEnabled(true);

   int selectedIndex = 0;
   for (int i = static_cast<int>(lowestRangeType) + 1;
      i <= static_cast<int>(otc::lastRangeValue(env)); ++i) {

      auto rangeType = otc::RangeType(i);
      if (checkLowestPossible(otc::getRange(rangeType).lower)) {
         break;
      }

      ui_->comboBoxRange->addItem(QString::fromStdString(otc::toString(rangeType)), i);

      if (rangeType == selectedRangeType) {
         selectedIndex = static_cast<int>(rangeType) - static_cast<int>(lowestRangeType);
      }
   }

   ui_->comboBoxRange->setCurrentIndex(selectedIndex);
}
