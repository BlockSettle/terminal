/*

***********************************************************************************
* Copyright (C) 2018 - 2021, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "CCWidget.h"

#include "ui_CCWidget.h"

#include "CCPortfolioModel.h"
#include "UiUtils.h"
#include "AssetManager.h"
#include "Wallets/SyncWalletsManager.h"

CCWidget::CCWidget(QWidget* parent)
   : QWidget(parent)
   , ui_(new Ui::CCWidget())
{
   ui_->setupUi(this);
   ui_->frameXbtValue->hide();
}

CCWidget::~CCWidget() = default;

void CCWidget::SetPortfolioModel(const std::shared_ptr<CCPortfolioModel>& model)
{
   ui_->treeViewCC->setModel(model.get());
   ui_->treeViewCC->header()->setSectionResizeMode(QHeaderView::Stretch);

   connect(model.get(), &CCPortfolioModel::rowsInserted, this, &CCWidget::onRowsInserted);
   connect(model.get(), &CCPortfolioModel::modelReset, this, [this]() { ui_->treeViewCC->expandAll(); });
}

void CCWidget::onWalletBalance(const bs::sync::WalletBalanceData& wbd)
{
   walletBalance_[wbd.id] = wbd.balTotal;
   updateTotalBalances();
}

void CCWidget::onPriceChanged(const std::string& currency, double price)
{
   auto& itPrice = fxPrices_[currency];
   if (itPrice != price) {
      itPrice = price;
      updateTotalBalances();
   }
}

void CCWidget::onBasePriceChanged(const std::string &currency, double price)
{
   baseCur_ = currency;
   if (basePrice_ != price) {
      basePrice_ = price;
      updateTotalBalances();
   }
}

void CCWidget::onBalance(const std::string& currency, double balance)
{
   if (balance > 0) {
      fxBalance_[currency] = balance;
      updateTotalBalances();
   }
}

void CCWidget::updateTotalAssets()
{
   int assets = 0;   //FIXME
   if (assets < 0) {
      ui_->labelTotalValue->setText(tr("<b>%1</b>").arg(tr("Loading...")));
   }
   else {
      ui_->labelTotalValue->setText(tr("<b>%1</b>").arg(UiUtils::displayAmount(assets)));
   }
}

void CCWidget::onRowsInserted(const QModelIndex &parent, int first, int last)
{
   Q_UNUSED(first)
   Q_UNUSED(last)
   ui_->treeViewCC->expandAll();
}

void CCWidget::updateTotalBalances()
{
   if (walletBalance_.empty() && fxBalance_.empty()) {
      ui_->labelTotalValue->setText(tr("<b>%1</b>").arg(tr("Loading...")));
   }
   else {
      double xbtBalance = 0;
      for (const auto& bal : walletBalance_) {
         xbtBalance += bal.second;
      }
      for (const auto& bal : fxBalance_) {
         try {
            xbtBalance += bal.second * fxPrices_.at(bal.first);
         }
         catch (const std::exception&) {}
      }
      ui_->labelTotalValue->setText(tr("<b>%1</b>").arg(UiUtils::displayAmount(xbtBalance)));
   }

   if (!walletBalance_.empty() && !baseCur_.empty()) {
      ui_->frameXbtValue->show();

      double xbtBalance = 0;
      for (const auto& bal : walletBalance_) {
         xbtBalance += bal.second;
      }
      xbtBalance *= basePrice_;
      ui_->labelXbtValue->setText(UiUtils::UnifyValueString(tr("<b>%1</b>").arg(QString::number(xbtBalance, 'f', 2))));
      ui_->labelBasePrice->setText(UiUtils::UnifyValueString(QString::number(basePrice_, 'f', 2)));
   }
}
