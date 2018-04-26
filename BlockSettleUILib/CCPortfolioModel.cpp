#include "CCPortfolioModel.h"

#include "AssetManager.h"
#include "UiUtils.h"
#include "WalletsManager.h"

QStandardItem* amountItem(const QString& text, bool bold = false)
{
   QFont font;
   font.setBold(bold);

   QStandardItem* item = new QStandardItem(text);
   item->setTextAlignment(Qt::AlignRight);
   item->setFont(font);

   return item;
}

CCPortfolioModel::CCPortfolioModel(std::shared_ptr<WalletsManager> walletsManager, std::shared_ptr<AssetManager> assetManager, QObject* parent)
   : QStandardItemModel(parent)
   , walletsManager_(walletsManager)
   , assetManager_(assetManager)
   , cashGroupName_(tr("Cash"))
{
   connect(walletsManager.get(), &WalletsManager::walletsReady, this, &CCPortfolioModel::updateBlockchainData);
   connect(walletsManager.get(), &WalletsManager::walletChanged, this, &CCPortfolioModel::updateBlockchainData);
   connect(walletsManager.get(), &WalletsManager::blockchainEvent, this, &CCPortfolioModel::updateBlockchainData);
   setHorizontalHeaderLabels({tr("Asset"), tr("Balance"), tr("Value (XBT)")});
   horizontalHeaderItem(1)->setTextAlignment(Qt::AlignRight);
   horizontalHeaderItem(2)->setTextAlignment(Qt::AlignRight);

   fontBold_.setBold(true);

   QList<QStandardItem*> xbtItems;
   xbtItems << new QStandardItem(UiUtils::XbtCurrency);
   xbtItems.last()->setFont(fontBold_);
   xbtItems << new QStandardItem();
   xbtItems << amountItem(UiUtils::displayAmount(walletsManager_->GetSpendableBalance()), true);
   appendRow(xbtItems);
   fillXbtWallets(xbtItems[0]);

   connect(assetManager_.get(), &AssetManager::securitiesChanged, this, &CCPortfolioModel::reloadSecurities);
   connect(assetManager_.get(), &AssetManager::securitiesReceived, this, &CCPortfolioModel::reloadSecurities);
   connect(assetManager_.get(), &AssetManager::balanceChanged, this, &CCPortfolioModel::updateCashAccountBalance);
   connect(assetManager_.get(), &AssetManager::priceChanged, [this](const std::string &currency) {
      if (assetManager_->getCCLotSize(currency)) {
         updatePrivateShare(currency, shareItems_);
      }
      else {
         updateCashAccountBalance(currency);
      }
   });
   connect(assetManager_.get(), &AssetManager::totalChanged, this, &CCPortfolioModel::updateCashTotalBalance);

   updateBlockchainData();
}

void CCPortfolioModel::updateCashTotalBalance()
{
   if (cashItems_.empty()) {
      return;
   }
   cashItems_.last()->setText(UiUtils::displayAmount(assetManager_->getCashTotal()));
}

void CCPortfolioModel::updateCashAccountBalance(const std::string &currency)
{
   if (currency == bs::network::XbtCurrency) {
      return;
   }

   if (cashItems_.empty()) {
      cashItems_ << new QStandardItem(cashGroupName_);
      cashItems_.last()->setFont(fontBold_);
      cashItems_ << new QStandardItem();
      cashItems_ << amountItem(tr("Not available"), true);
      appendRow(cashItems_);
   }
   const double balance = assetManager_->getBalance(currency);
   const double price = assetManager_->getPrice(currency);
   int index = -1;

   for (int i = 0; i < cashItems_.first()->rowCount(); i++) {
      if (cashItems_.first()->child(i, 0)->text().toStdString() == currency) {
         index = i;
         break;
      }
   }

   if (index < 0) {
      QList<QStandardItem*> currencyItems;
      currencyItems << new QStandardItem(QString::fromStdString(currency));
      currencyItems << amountItem(UiUtils::displayCurrencyAmount(balance));
      currencyItems << amountItem(UiUtils::displayAmount(price * balance));

      index = cashItems_.first()->rowCount();
      cashItems_.first()->appendRow(currencyItems);
   }

   cashItems_.first()->child(index, 1)->setText(UiUtils::displayCurrencyAmount(balance));
   cashItems_.first()->child(index, 2)->setText(UiUtils::displayAmount(price * balance));
}

void CCPortfolioModel::reloadSecurities()
{
   const auto &currencies = assetManager_->currencies();
   if (currencies.empty()) {
      cashItems_.clear();
      int cashRow = -1;
      for (int row = 0; row < rowCount(); row++) {
         if (item(row)->text() == cashGroupName_) {
            cashRow = row;
            break;
         }
      }
      if (cashRow >= 0) {
         removeRow(cashRow);
      }
   }
   else {
      for (const auto &cur : currencies) {
         updateCashAccountBalance(cur);
      }
      updateCashTotalBalance();
   }
}

void CCPortfolioModel::updatePrivateShares()
{
   const auto &privShares = assetManager_->privateShares();
   if (privShares.empty()) {
      return;
   }

   if (shareItems_.empty()) {
      shareItems_ << new QStandardItem(tr("Private Shares"));
      shareItems_.last()->setFont(fontBold_);
      shareItems_ << new QStandardItem();
      shareItems_ << amountItem(tr("Not available"), true);
      shareItems_.last()->setText(UiUtils::displayAmount(assetManager_->getCCTotal()));
      appendRow(shareItems_);

      connect(assetManager_.get(), &AssetManager::totalChanged, [this]() {
         shareItems_.last()->setText(UiUtils::displayAmount(assetManager_->getCCTotal()));
      });
   }

   for (const auto &privShare : privShares) {
      updatePrivateShare(privShare, shareItems_);
   }
}

void CCPortfolioModel::updatePrivateShare(const std::string &cc, QList<QStandardItem*> &shareItems)
{
   if (shareItems.empty()) {
      return;
   }

   int index = -1;
   for (int i = 0; i < shareItems.first()->rowCount(); i++) {
      if (shareItems.first()->child(i, 0)->text().toStdString() == cc) {
         index = i;
         break;
      }
   }

   if (index < 0) {
      QList<QStandardItem*> singleShareItems;
      singleShareItems << new QStandardItem(QString::fromStdString(cc));
      singleShareItems << amountItem(tr("Not available"));
      singleShareItems << amountItem(tr("Not available"));

      index = shareItems.first()->rowCount();
      shareItems.first()->appendRow(singleShareItems);
   }

   if (index >= 0) {
      const auto balance = assetManager_->getBalance(cc);
      shareItems.first()->child(index, 1)->setText(UiUtils::displayCCAmount(balance));
      shareItems.first()->child(index, 2)->setText(UiUtils::displayAmount(balance * assetManager_->getPrice(cc)));
   }
}

void CCPortfolioModel::updateBlockchainData()
{
   item(0,2)->setText(UiUtils::displayAmount(walletsManager_->GetSpendableBalance()));

   const auto xbtItem = item(0,0);
   xbtItem->removeRows(0, xbtItem->rowCount());
   fillXbtWallets(xbtItem);

   updatePrivateShares();
}

void CCPortfolioModel::fillXbtWallets(QStandardItem *item)
{
   for (size_t i = 0; i < walletsManager_->GetWalletsCount(); i++) {
      const auto &wallet = walletsManager_->GetWallet(i);
      if (!wallet || (wallet->GetType() != bs::wallet::Type::Bitcoin)) {
         continue;
      }
      QList<QStandardItem*> walletItems;
      walletItems << new QStandardItem(QString::fromStdString(wallet->GetWalletName()));
      walletItems << new QStandardItem();
      walletItems << amountItem(UiUtils::displayAmount(wallet->GetSpendableBalance()));
      item->appendRow(walletItems);
   }
}

std::shared_ptr<AssetManager> CCPortfolioModel::assetManager()
{
   return assetManager_;
}
