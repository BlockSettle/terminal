/*

***********************************************************************************
* Copyright (C) 2019 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "RFQDealerReply.h"
#include "ui_RFQDealerReply.h"

#include <spdlog/spdlog.h>

#include <chrono>

#include <QComboBox>
#include <QLineEdit>

#include "ApplicationSettings.h"
#include "AssetManager.h"
#include "AuthAddressManager.h"
#include "AutoSignQuoteProvider.h"
#include "BSErrorCodeStrings.h"
#include "BSMessageBox.h"
#include "CoinControlDialog.h"
#include "CoinControlWidget.h"
#include "CurrencyPair.h"
#include "CustomControls/CustomComboBox.h"
#include "FastLock.h"
#include "QuoteProvider.h"
#include "SelectedTransactionInputs.h"
#include "SignContainer.h"
#include "TradesUtils.h"
#include "TxClasses.h"
#include "UiUtils.h"
#include "UserScriptRunner.h"
#include "UtxoReservationManager.h"
#include "Wallets/SyncHDWallet.h"
#include "Wallets/SyncWalletsManager.h"

namespace {
   const QString kNoBalanceAvailable = QLatin1String("-");
   const QString kReservedBalance = QLatin1String("Reserved input balance");
   const QString kAvailableBalance = QLatin1String("Available balance");
}

using namespace bs::ui;

RFQDealerReply::RFQDealerReply(QWidget* parent)
   : QWidget(parent)
   , ui_(new Ui::RFQDealerReply())
{
   ui_->setupUi(this);
   initUi();

   connect(ui_->spinBoxBidPx, static_cast<void(QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), this, &RFQDealerReply::priceChanged);
   connect(ui_->spinBoxOfferPx, static_cast<void(QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), this, &RFQDealerReply::priceChanged);

   ui_->spinBoxBidPx->installEventFilter(this);
   ui_->spinBoxOfferPx->installEventFilter(this);

   connect(ui_->pushButtonSubmit, &QPushButton::clicked, this, &RFQDealerReply::submitButtonClicked);
   connect(ui_->pushButtonPull, &QPushButton::clicked, this, &RFQDealerReply::pullButtonClicked);
   connect(ui_->toolButtonXBTInputsSend, &QPushButton::clicked, this, &RFQDealerReply::showCoinControl);

   connect(ui_->comboBoxXbtWallet, qOverload<int>(&QComboBox::currentIndexChanged), this, &RFQDealerReply::walletSelected);
   connect(ui_->authenticationAddressComboBox, qOverload<int>(&QComboBox::currentIndexChanged), this, &RFQDealerReply::onAuthAddrChanged);

   ui_->groupBoxSettlementInputs->hide();
}

RFQDealerReply::~RFQDealerReply() = default;

void RFQDealerReply::init(const std::shared_ptr<spdlog::logger> logger
   , const std::shared_ptr<AuthAddressManager> &authAddressManager
   , const std::shared_ptr<AssetManager>& assetManager
   , const std::shared_ptr<QuoteProvider>& quoteProvider
   , const std::shared_ptr<ApplicationSettings> &appSettings
   , const std::shared_ptr<ConnectionManager> &connectionManager
   , const std::shared_ptr<SignContainer> &container
   , const std::shared_ptr<ArmoryConnection> &armory
   , const std::shared_ptr<AutoSignScriptProvider> &autoSignProvider
   , const std::shared_ptr<bs::UTXOReservationManager> &utxoReservationManager)
{
   logger_ = logger;
   assetManager_ = assetManager;
   quoteProvider_ = quoteProvider;
   authAddressManager_ = authAddressManager;
   appSettings_ = appSettings;
   signingContainer_ = container;
   armory_ = armory;
   connectionManager_ = connectionManager;
   autoSignProvider_ = autoSignProvider;
   utxoReservationManager_ = utxoReservationManager;

   connect((AQScriptRunner *)autoSignProvider_->scriptRunner(), &AQScriptRunner::sendQuote
      , this, &RFQDealerReply::onAQReply, Qt::QueuedConnection);
   connect((AQScriptRunner *)autoSignProvider_->scriptRunner(), &AQScriptRunner::pullQuoteNotif
      , this, &RFQDealerReply::pullQuoteNotif, Qt::QueuedConnection);

   connect(autoSignProvider_.get(), &AutoSignScriptProvider::autoSignStateChanged,
      this, &RFQDealerReply::onAutoSignStateChanged, Qt::QueuedConnection);
   connect(utxoReservationManager_.get(), &bs::UTXOReservationManager::availableUtxoChanged,
      this, &RFQDealerReply::onUTXOReservationChanged);
}

void bs::ui::RFQDealerReply::init(const std::shared_ptr<spdlog::logger>& logger)
{
   logger_ = logger;
}

void RFQDealerReply::initUi()
{
   invalidBalanceFont_ = ui_->labelBalanceValue->font();
   invalidBalanceFont_.setStrikeOut(true);

   ui_->authenticationAddressLabel->hide();
   ui_->authenticationAddressComboBox->hide();
   ui_->pushButtonSubmit->setEnabled(false);
   ui_->pushButtonPull->setEnabled(false);
   ui_->widgetWallet->hide();

   ui_->spinBoxBidPx->clear();
   ui_->spinBoxOfferPx->clear();
   ui_->spinBoxBidPx->setEnabled(false);
   ui_->spinBoxOfferPx->setEnabled(false);

   ui_->labelProductGroup->clear();

   validateGUI();
}

void RFQDealerReply::setWalletsManager(const std::shared_ptr<bs::sync::WalletsManager> &walletsManager)
{
   walletsManager_ = walletsManager;
   validateGUI();

   connect(walletsManager_.get(), &bs::sync::WalletsManager::CCLeafCreated, this, &RFQDealerReply::onHDLeafCreated);
   connect(walletsManager_.get(), &bs::sync::WalletsManager::CCLeafCreateFailed, this, &RFQDealerReply::onCreateHDWalletError);

   if (autoSignProvider_->scriptRunner()) {
      autoSignProvider_->scriptRunner()->setWalletsManager(walletsManager_);
   }

   auto updateAuthAddresses = [this] {
      UiUtils::fillAuthAddressesComboBoxWithSubmitted(ui_->authenticationAddressComboBox, authAddressManager_);
      onAuthAddrChanged(ui_->authenticationAddressComboBox->currentIndex());
   };
   updateAuthAddresses();
   connect(authAddressManager_.get(), &AuthAddressManager::AddressListUpdated, this, updateAuthAddresses);
}

CustomDoubleSpinBox* RFQDealerReply::bidSpinBox() const
{
   return ui_->spinBoxBidPx;
}

CustomDoubleSpinBox* RFQDealerReply::offerSpinBox() const
{
   return ui_->spinBoxOfferPx;
}

QPushButton* RFQDealerReply::pullButton() const
{
   return ui_->pushButtonPull;
}

QPushButton* RFQDealerReply::quoteButton() const
{
   return ui_->pushButtonSubmit;
}

void RFQDealerReply::updateRespQuantity()
{
   if (currentQRN_.empty()) {
      ui_->labelRespQty->clear();
      return;
   }

   if (product_ == bs::network::XbtCurrency) {
      ui_->labelRespQty->setText(UiUtils::displayAmount(getValue()));
   } else {
      ui_->labelRespQty->setText(UiUtils::displayCurrencyAmount(getValue()));
   }
}

void RFQDealerReply::reset()
{
   payInRecipId_ = UINT_MAX;
   if (currentQRN_.empty()) {
      ui_->labelProductGroup->clear();
      ui_->labelSecurity->clear();
      ui_->labelReqProduct->clear();
      ui_->labelReqSide->clear();
      ui_->labelReqQty->clear();
      ui_->labelRespProduct->clear();
      ui_->labelQuantity->clear();
      ui_->labelProduct->clear();
      indicBid_ = indicAsk_ = 0;
      selectedXbtInputs_.clear();
      selectedXbtRes_.release();
      setBalanceOk(true);
   }
   else {
      CurrencyPair cp(currentQRN_.security);
      baseProduct_ = cp.NumCurrency();
      product_ = cp.ContraCurrency(currentQRN_.product);

      if (currentQRN_.assetType == bs::network::Asset::Type::Undefined) {
         logger_->error("[RFQDealerReply::reset] could not get asset type for {}", currentQRN_.security);
      }
      const auto priceDecimals = UiUtils::GetPricePrecisionForAssetType(currentQRN_.assetType);
      ui_->spinBoxBidPx->setDecimals(priceDecimals);
      ui_->spinBoxOfferPx->setDecimals(priceDecimals);
      ui_->spinBoxBidPx->setSingleStep(std::pow(10, -priceDecimals));
      ui_->spinBoxOfferPx->setSingleStep(std::pow(10, -priceDecimals));

      const auto priceWidget = getActivePriceWidget();
      ui_->spinBoxBidPx->setEnabled(priceWidget == ui_->spinBoxBidPx);
      ui_->spinBoxOfferPx->setEnabled(priceWidget == ui_->spinBoxOfferPx);
/*      priceWidget->setFocus();
      priceWidget->selectAll();*/

      ui_->labelProductGroup->setText(tr(bs::network::Asset::toString(currentQRN_.assetType)));
      ui_->labelSecurity->setText(QString::fromStdString(currentQRN_.security));
      ui_->labelReqProduct->setText(QString::fromStdString(currentQRN_.product));
      ui_->labelReqSide->setText(tr(bs::network::Side::toString(currentQRN_.side)));
      ui_->labelReqQty->setText(UiUtils::displayAmountForProduct(currentQRN_.quantity
         , QString::fromStdString(currentQRN_.product), currentQRN_.assetType));

      ui_->labelRespProduct->setText(QString::fromStdString(product_));
      ui_->labelQuantity->setText(UiUtils::displayAmountForProduct(currentQRN_.quantity
         , QString::fromStdString(currentQRN_.product), currentQRN_.assetType));
      ui_->labelProduct->setText(QString::fromStdString(currentQRN_.product));

      if (sentNotifs_.count(currentQRN_.quoteRequestId) == 0) {
         selectedXbtInputs_.clear();
      }
      else {
         const auto* lastSettlement = getLastUTXOReplyCb_(currentQRN_.settlementId);
         if (lastSettlement && selectedXbtInputs_ != *lastSettlement) {
            selectedXbtInputs_ = *lastSettlement;
         }
      }
   }

   updateRespQuantity();
   updateSpinboxes();
   refreshSettlementDetails();
}

void RFQDealerReply::quoteReqNotifStatusChanged(const bs::network::QuoteReqNotification &qrn)
{
   if (!QuoteProvider::isRepliableStatus(qrn.status)) {
      sentNotifs_.erase(qrn.quoteRequestId);
      addresses_.erase(qrn.quoteRequestId);
   }

   if (qrn.quoteRequestId == currentQRN_.quoteRequestId) {
      updateQuoteReqNotification(qrn);
   }

   refreshSettlementDetails();
}

void RFQDealerReply::setQuoteReqNotification(const bs::network::QuoteReqNotification &qrn
   , double indicBid, double indicAsk)
{
   indicBid_ = indicBid;
   indicAsk_ = indicAsk;

   updateQuoteReqNotification(qrn);
}

void RFQDealerReply::updateQuoteReqNotification(const bs::network::QuoteReqNotification &qrn)
{
   const auto &oldReqId = currentQRN_.quoteRequestId;
   const bool qrnChanged = (oldReqId != qrn.quoteRequestId);
   currentQRN_ = qrn;

   const bool isXBT = (qrn.assetType == bs::network::Asset::SpotXBT);
   const bool isPrivMkt = (qrn.assetType == bs::network::Asset::PrivateMarket);

   dealerSellXBT_ = (isXBT || isPrivMkt) && ((qrn.product == bs::network::XbtCurrency) != (qrn.side == bs::network::Side::Sell));

   ui_->authenticationAddressLabel->setVisible(isXBT);
   ui_->authenticationAddressComboBox->setVisible(isXBT);
   ui_->widgetWallet->setVisible(isXBT || isPrivMkt);
   ui_->toolButtonXBTInputsSend->setVisible(dealerSellXBT_ && isXBT);
   ui_->labelWallet->setText(dealerSellXBT_ ? tr("Payment Wallet") : tr("Receiving Wallet"));

   updateUiWalletFor(qrn);

   if (qrnChanged) {
      reset();
   }

   if (qrn.assetType == bs::network::Asset::SpotFX ||
      qrn.assetType == bs::network::Asset::Undefined) {
         ui_->groupBoxSettlementInputs->hide();
   } else {
      ui_->groupBoxSettlementInputs->show();
   }

   updateSubmitButton();
}

std::shared_ptr<bs::sync::Wallet> RFQDealerReply::getCCWallet(const std::string &cc) const
{
   return walletsManager_->getCCWallet(cc);
}

std::shared_ptr<bs::sync::Wallet> RFQDealerReply::getCCWallet(const bs::network::QuoteReqNotification &qrn) const
{
   return getCCWallet(qrn.product);
}

void RFQDealerReply::getAddress(const std::string &quoteRequestId, const std::shared_ptr<bs::sync::Wallet> &wallet
   , AddressType type, std::function<void(bs::Address)> cb)
{
   if (!wallet) {
      cb({});
      return;
   }

   auto address = addresses_[quoteRequestId][wallet->walletId()].at(static_cast<size_t>(type));
   if (!address.empty()) {
      cb(address);
      return;
   }

   auto cbWrap = [this, quoteRequestId, wallet, cb = std::move(cb), type](const bs::Address &addr) {
      if (wallet->type() != bs::core::wallet::Type::ColorCoin && type == AddressType::Recv) {
         wallet->setAddressComment(addr, bs::sync::wallet::Comment::toString(bs::sync::wallet::Comment::SettlementPayOut));
      }
      addresses_[quoteRequestId][wallet->walletId()][static_cast<size_t>(type)] = addr;
      cb(addr);
   };
   switch (type) {
      case AddressType::Recv:
         // BST-2474: All addresses related to trading, not just change addresses, should use internal addresses.
         // CC wallets have only external addresses so getNewIntAddress will return external address instead but that's not a problem.
         wallet->getNewIntAddress(cbWrap);
         break;
      case AddressType::Change:
         wallet->getNewChangeAddress(cbWrap);
         break;
   }
}

void RFQDealerReply::updateUiWalletFor(const bs::network::QuoteReqNotification &qrn)
{
   if (armory_ && (armory_->state() != ArmoryState::Ready)) {
      return;
   }
   if (qrn.assetType == bs::network::Asset::PrivateMarket) {
      if (qrn.side == bs::network::Side::Sell) {
         const auto &ccWallet = getCCWallet(qrn.product);
         if (!ccWallet) {
            if (signingContainer_ && !signingContainer_->isOffline()) {
               MessageBoxCCWalletQuestion qryCCWallet(QString::fromStdString(qrn.product), this);

               if (qryCCWallet.exec() == QDialog::Accepted) {
                  if (!walletsManager_->CreateCCLeaf(qrn.product)) {
                     BSMessageBox errorMessage(BSMessageBox::critical, tr("Internal error")
                        , tr("Failed create CC subwallet.")
                        , this);
                     errorMessage.exec();
                  }
               }
            } else {
               BSMessageBox errorMessage(BSMessageBox::critical, tr("Signer not connected")
                  , tr("Could not create CC subwallet.")
                  , this);
               errorMessage.exec();
            }
         }
      }
      updateWalletsList((qrn.side == bs::network::Side::Sell) ? UiUtils::WalletsTypes::Full : UiUtils::WalletsTypes::All);
   } else if (qrn.assetType == bs::network::Asset::SpotXBT) {
      updateWalletsList((qrn.side == bs::network::Side::Sell)
         ? (UiUtils::WalletsTypes::Full | UiUtils::WalletsTypes::HardwareSW)
         : UiUtils::WalletsTypes::All);
   }
}

void RFQDealerReply::priceChanged()
{
   updateRespQuantity();
   updateSubmitButton();
}

void RFQDealerReply::onAuthAddrChanged(int index)
{
   auto addressString = ui_->authenticationAddressComboBox->itemText(index).toStdString();
   if (addressString.empty()) {
      return;
   }
   authAddr_  = bs::Address::fromAddressString(addressString);
   authKey_.clear();

   if (authAddr_.empty()) {
      return;
   }
   const auto settlLeaf = walletsManager_->getSettlementLeaf(authAddr_);

   const auto &cbPubKey = [this](const SecureBinaryData &pubKey) {
      authKey_ = pubKey.toHexStr();
      QMetaObject::invokeMethod(this, &RFQDealerReply::updateSubmitButton);
   };

   if (settlLeaf) {
      settlLeaf->getRootPubkey(cbPubKey);
   } else {
      walletsManager_->createSettlementLeaf(authAddr_, cbPubKey);
   }
}

void RFQDealerReply::updateSubmitButton()
{
   if (!currentQRN_.empty() && activeQuoteSubmits_.find(currentQRN_.quoteRequestId) != activeQuoteSubmits_.end()) {
      // Do not allow re-enter into submitReply as it could cause problems
      ui_->pushButtonSubmit->setEnabled(false);
      ui_->pushButtonPull->setEnabled(false);
      return;
   }

   updateBalanceLabel();
   bool isQRNRepliable = (!currentQRN_.empty() && QuoteProvider::isRepliableStatus(currentQRN_.status));
   if ((currentQRN_.assetType != bs::network::Asset::SpotFX)
      && signingContainer_ && signingContainer_->isOffline()) {
      isQRNRepliable = false;
   }

   ui_->pushButtonSubmit->setEnabled(isQRNRepliable);
   ui_->pushButtonPull->setEnabled(isQRNRepliable);
   if (!isQRNRepliable) {
      ui_->spinBoxBidPx->setEnabled(false);
      ui_->spinBoxOfferPx->setEnabled(false);
      return;
   }

   const auto itQN = sentNotifs_.find(currentQRN_.quoteRequestId);
   ui_->pushButtonPull->setEnabled(itQN != sentNotifs_.end());

   const auto price = getPrice();
   if (qFuzzyIsNull(price) || ((itQN != sentNotifs_.end()) && qFuzzyCompare(itQN->second, price))) {
      ui_->pushButtonSubmit->setEnabled(false);
      return;
   }

   if ((currentQRN_.assetType == bs::network::Asset::SpotXBT) && authKey_.empty()) {
      ui_->pushButtonSubmit->setEnabled(false);
      return;
   }

   const bool isBalanceOk = assetManager_ ? checkBalance() : true;
   ui_->pushButtonSubmit->setEnabled(isBalanceOk);
   setBalanceOk(isBalanceOk);
}

void RFQDealerReply::setBalanceOk(bool ok)
{
   if (!ok) {
      QPalette palette = ui_->labelRespQty->palette();
      palette.setColor(QPalette::WindowText, Qt::red);
      ui_->labelRespQty->setPalette(palette);
      return;
   }
   ui_->labelRespQty->setPalette(QPalette());
}

bool RFQDealerReply::checkBalance() const
{
   if (!assetManager_) {
      return false;
   }

   // #UTXO_MANAGER: Balance check should account for fee?

   if ((currentQRN_.side == bs::network::Side::Buy) != (product_ == baseProduct_)) {
      const auto amount = getAmount();
      if (currentQRN_.assetType == bs::network::Asset::SpotXBT) {
         return amount <= getXbtBalance().GetValueBitcoin();
      } else if (currentQRN_.assetType == bs::network::Asset::PrivateMarket) {
         return amount <= getPrivateMarketCoinBalance();
      }
      const auto product = (product_ == baseProduct_) ? product_ : currentQRN_.product;
      return assetManager_->checkBalance(product, amount);
   } else if ((currentQRN_.side == bs::network::Side::Buy) && (product_ == baseProduct_)) {
      return assetManager_->checkBalance(currentQRN_.product, currentQRN_.quantity);
   }

   if (currentQRN_.assetType == bs::network::Asset::PrivateMarket) {
      return currentQRN_.quantity * getPrice() <= getXbtBalance().GetValueBitcoin();
   }

   const double value = getValue();
   if (qFuzzyIsNull(value)) {
      return true;
   }
   const bool isXbt = (currentQRN_.assetType == bs::network::Asset::PrivateMarket) ||
      ((currentQRN_.assetType == bs::network::Asset::SpotXBT) && (product_ == baseProduct_));
   if (isXbt) {
      return value <= getXbtBalance().GetValueBitcoin();
   }
   return assetManager_->checkBalance(product_, value);
}

void RFQDealerReply::walletSelected(int index)
{
   reset();
   updateSubmitButton();
}

QDoubleSpinBox *RFQDealerReply::getActivePriceWidget() const
{
   if (currentQRN_.empty()) {
      return nullptr;
   }

   if (baseProduct_ == currentQRN_.product) {
      if (currentQRN_.side == bs::network::Side::Buy) {
         return ui_->spinBoxOfferPx;
      }
      return ui_->spinBoxBidPx;
   }
   else {
      if (currentQRN_.side == bs::network::Side::Buy) {
         return ui_->spinBoxBidPx;
      }
      return ui_->spinBoxOfferPx;
   }
}

double RFQDealerReply::getPrice() const
{
   const auto spinBox = getActivePriceWidget();
   return spinBox ? spinBox->value() : 0;
}

double RFQDealerReply::getValue() const
{
   const double price = getPrice();
   if (baseProduct_ == product_) {
      if (!qFuzzyIsNull(price)) {
         return  currentQRN_.quantity / price;
      }
      return 0;
   }
   return price * currentQRN_.quantity;
}

double RFQDealerReply::getAmount() const
{
   if (baseProduct_ == product_) {
      const double price = getPrice();
      if (!qFuzzyIsNull(price)) {
         return  currentQRN_.quantity / price;
      }
      return 0;
   }
   return currentQRN_.quantity;
}

std::shared_ptr<bs::sync::hd::Wallet> RFQDealerReply::getSelectedXbtWallet(ReplyType replyType) const
{
   if (!walletsManager_) {
      return nullptr;
   }
   return walletsManager_->getHDWalletById(getSelectedXbtWalletId(replyType));
}

bs::Address RFQDealerReply::selectedAuthAddress(ReplyType replyType) const
{
   if (authAddressManager_ && (replyType == ReplyType::Script)) {
      return authAddressManager_->getDefault();
   }
   return authAddr_;
}

std::vector<UTXO> RFQDealerReply::selectedXbtInputs(ReplyType replyType) const
{
   if (replyType == ReplyType::Script) {
      return {};
   }

   return selectedXbtInputs_;
}

void RFQDealerReply::setSubmitQuoteNotifCb(RFQDealerReply::SubmitQuoteNotifCb cb)
{
   submitQuoteNotifCb_ = std::move(cb);
}

void RFQDealerReply::setResetCurrentReservation(RFQDealerReply::ResetCurrentReservationCb cb)
{
   resetCurrentReservationCb_ = std::move(cb);
}

void bs::ui::RFQDealerReply::setGetLastSettlementReply(GetLastUTXOReplyCb cb)
{
   getLastUTXOReplyCb_ = std::move(cb);
}

void bs::ui::RFQDealerReply::onParentAboutToHide()
{
   if (sentNotifs_.count(currentQRN_.quoteRequestId) == 0) {
      selectedXbtInputs_.clear();
   }
   selectedXbtRes_.release();
}

void bs::ui::RFQDealerReply::onHDWallet(const bs::sync::HDWalletData& wallet)
{
   const auto& it = std::find_if(wallets_.cbegin(), wallets_.cend()
      , [wallet](const bs::sync::HDWalletData& w) { return (wallet.id == w.id); });
   if (it == wallets_.end()) {
      wallets_.push_back(wallet);
   } else {
      wallets_.emplace(it, wallet);
   }

}

void bs::ui::RFQDealerReply::onBalance(const std::string& currency, double balance)
{
   balances_[currency] = balance;
}

void bs::ui::RFQDealerReply::onWalletBalance(const bs::sync::WalletBalanceData& wbd)
{
   balances_[wbd.id] = wbd.balSpendable;
}

void bs::ui::RFQDealerReply::onAuthKey(const bs::Address& addr, const BinaryData& authKey)
{
   if (addr == authAddr_) {
      logger_->debug("[{}] got auth key: {}", __func__, authKey.toHexStr());
      authKey_ = authKey.toHexStr();
   }
}

void RFQDealerReply::submitReply(const bs::network::QuoteReqNotification &qrn
   , double price, ReplyType replyType)
{
   if (qFuzzyIsNull(price)) {
      SPDLOG_LOGGER_ERROR(logger_, "invalid price");
      return;
   }

   const auto itQN = sentNotifs_.find(qrn.quoteRequestId);
   if (itQN != sentNotifs_.end() && itQN->second == price) {
      SPDLOG_LOGGER_ERROR(logger_, "quote have been already sent");
      return;
   }

   auto replyData = std::make_shared<SubmitQuoteReplyData>();
   replyData->qn = bs::network::QuoteNotification(qrn, authKey_, price, "");

   if (qrn.assetType != bs::network::Asset::SpotFX) {
      replyData->walletPurpose = UiUtils::getSelectedHwPurpose(ui_->comboBoxXbtWallet);;
      if (walletsManager_) {
         replyData->xbtWallet = getSelectedXbtWallet(replyType);
         if (!replyData->xbtWallet) {
            SPDLOG_LOGGER_ERROR(logger_, "can't submit CC/XBT reply without XBT wallet");
            return;
         }
      }
      else {
         replyData->xbtWalletId = getSelectedXbtWalletId(replyType);
      }
   }

   if (qrn.assetType == bs::network::Asset::SpotXBT) {
      replyData->authAddr = selectedAuthAddress(replyType);
      if (!replyData->authAddr.isValid()) {
         SPDLOG_LOGGER_ERROR(logger_, "can't submit XBT without valid auth address");
         return;
      }

      bs::XBTAmount minXbtAmount;
      if (utxoReservationManager_) {
         minXbtAmount = bs::tradeutils::minXbtAmount(utxoReservationManager_->feeRatePb());
      }
      else {
         minXbtAmount = bs::tradeutils::minXbtAmount(1); //FIXME: should populate PB fee rate somehow
      }
      auto xbtAmount = XBTAmount(qrn.product == bs::network::XbtCurrency ? qrn.quantity : qrn.quantity / price);
      if (xbtAmount.GetValue() < minXbtAmount.GetValue()) {
         SPDLOG_LOGGER_ERROR(logger_, "XBT amount is too low to cover network fee: {}, min. amount: {}"
            , xbtAmount.GetValue(), minXbtAmount.GetValue());
         return;
      }
   }

   auto it = activeQuoteSubmits_.find(replyData->qn.quoteRequestId);
   if (it != activeQuoteSubmits_.end()) {
      SPDLOG_LOGGER_ERROR(logger_, "quote submit already active for quote request '{}'"
         , replyData->qn.quoteRequestId);
      return;
   }
   activeQuoteSubmits_.insert(replyData->qn.quoteRequestId);
   updateSubmitButton();

   switch (qrn.assetType) {
      case bs::network::Asset::SpotFX: {
         submit(price, replyData);
         break;
      }

      case bs::network::Asset::SpotXBT: {
         reserveBestUtxoSetAndSubmit(qrn.quantity, price, replyData, replyType);
         break;
      }

      case bs::network::Asset::PrivateMarket: {
         auto ccWallet = getCCWallet(qrn);
         if (!ccWallet) {
            SPDLOG_LOGGER_ERROR(logger_, "can't find required CC wallet ({})", qrn.product);
            return;
         }

         const bool isSpendCC = qrn.side == bs::network::Side::Buy;
         uint64_t spendVal;
         if (isSpendCC) {
            spendVal = static_cast<uint64_t>(qrn.quantity * assetManager_->getCCLotSize(qrn.product));
         } else {
            spendVal = bs::XBTAmount(price * qrn.quantity).GetValue();
         }

         auto xbtLeaves = replyData->xbtWallet->getGroup(bs::sync::hd::Wallet::getXBTGroupType())->getLeaves();
         if (xbtLeaves.empty()) {
            SPDLOG_LOGGER_ERROR(logger_, "empty XBT leaves in wallet {}", replyData->xbtWallet->walletId());
            return;
         }
         auto  xbtWallets = std::vector<std::shared_ptr<bs::sync::Wallet>>(xbtLeaves.begin(), xbtLeaves.end());
         auto xbtWallet = xbtWallets.front();

         const auto &spendWallet = isSpendCC ? ccWallet : xbtWallet;
         const auto &recvWallet = isSpendCC ? xbtWallet : ccWallet;

         preparingCCRequest_.insert(replyData->qn.quoteRequestId);
         auto recvAddrCb = [this, replyData, qrn, spendWallet, spendVal, isSpendCC, ccWallet, xbtWallets, price](const bs::Address &addr) {
            replyData->qn.receiptAddress = addr.display();
            replyData->qn.reqAuthKey = qrn.requestorRecvAddress;

            const auto &cbFee = [this, qrn, spendVal, spendWallet, isSpendCC, replyData, ccWallet, xbtWallets, price](float feePerByteArmory) {
               auto feePerByte = std::max(feePerByteArmory, utxoReservationManager_->feeRatePb());
               auto inputsCb = [this, qrn, feePerByte, replyData, spendVal, spendWallet, isSpendCC, price]
                  (const std::map<UTXO, std::string> &inputs)
               {
                  QMetaObject::invokeMethod(this, [this, feePerByte, qrn, replyData, spendVal, spendWallet, isSpendCC, inputs, price] {
                     const auto &cbChangeAddr = [this, feePerByte, qrn, replyData, spendVal, spendWallet, inputs, price, isSpendCC]
                        (const bs::Address &changeAddress)
                     {
                        try {
                           //group 1 for cc, group 2 for xbt
                           unsigned spendGroup = isSpendCC ? RECIP_GROUP_SPEND_1 : RECIP_GROUP_SPEND_2;
                           unsigned changGroup = isSpendCC ? RECIP_GROUP_CHANG_1 : RECIP_GROUP_CHANG_2;
                           
                           std::map<unsigned, std::vector<std::shared_ptr<ArmorySigner::ScriptRecipient>>> recipientMap;
                           const auto recipient = bs::Address::fromAddressString(qrn.requestorRecvAddress).getRecipient(bs::XBTAmount{ spendVal });
                           std::vector<std::shared_ptr<ArmorySigner::ScriptRecipient>> recVec({recipient});
                           recipientMap.emplace(spendGroup, std::move(recVec));
                           

                           Codec_SignerState::SignerState state;
                           state.ParseFromString(BinaryData::CreateFromHex(qrn.requestorAuthPublicKey).toBinStr());
                           auto txReq = bs::sync::WalletsManager::createPartialTXRequest(spendVal, inputs
                              , changeAddress
                              , isSpendCC ? 0 : feePerByte, armory_->topBlock()
                              , recipientMap, changGroup, state
                              , false, UINT32_MAX, logger_);
                           logger_->debug("[RFQDealerReply::submitReply] {} input[s], fpb={}, recip={}, "
                              "change amount={}, prevPart={}", inputs.size(), feePerByte
                              , bs::Address::fromAddressString(qrn.requestorRecvAddress).display()
                              , txReq.change.value, qrn.requestorAuthPublicKey);

                           signingContainer_->resolvePublicSpenders(txReq, [replyData, this, price, txReq]
                              (bs::error::ErrorCode result, const Codec_SignerState::SignerState &state)
                           {
                              if (preparingCCRequest_.count(replyData->qn.quoteRequestId) == 0) {
                                 return;
                              }

                              if (result == bs::error::ErrorCode::NoError) {
                                 replyData->qn.transactionData = BinaryData::fromString(state.SerializeAsString()).toHexStr();
                                 replyData->utxoRes = utxoReservationManager_->makeNewReservation(
                                    txReq.getInputs(nullptr), replyData->qn.quoteRequestId);
                                 submit(price, replyData);
                              }
                              else {
                                 SPDLOG_LOGGER_ERROR(logger_, "error resolving public spenders: {}"
                                    , bs::error::ErrorCodeToString(result).toStdString());
                              }
                              preparingCCRequest_.erase(replyData->qn.quoteRequestId);
                           });
                        } catch (const std::exception &e) {
                           SPDLOG_LOGGER_ERROR(logger_, "error creating own unsigned half: {}", e.what());
                           return;
                        }
                     };

                     if (isSpendCC) {
                        uint64_t inputsVal = 0;
                        for (const auto &input : inputs) {
                           inputsVal += input.first.getValue();
                        }
                        if (inputsVal == spendVal) {
                           cbChangeAddr({});
                           return;
                        }
                     }
                     getAddress(qrn.quoteRequestId, spendWallet, AddressType::Change, cbChangeAddr);
                  });
               };

               // Try to reset current reservation if needed when user sends another quote
               resetCurrentReservationCb_(replyData);

               if (isSpendCC) {
                  const auto  inputsWrapCb = [inputsCb, ccWallet](const std::vector<UTXO> &utxos) {
                     std::map<UTXO, std::string> inputs;
                     for (const auto &utxo : utxos) {
                        inputs[utxo] = ccWallet->walletId();
                     }
                     inputsCb(inputs);
                  };
                  // For CC search for exact amount (preferable without change)
                  ccWallet->getSpendableTxOutList(inputsWrapCb, spendVal, true);
               } else {
                  // For XBT request all available inputs as we don't know fee yet (createPartialTXRequest will use correct inputs if fee rate is set)
                  std::vector<UTXO> utxos;
                  if (!replyData->xbtWallet->canMixLeaves()) {
                     auto purpose = UiUtils::getSelectedHwPurpose(ui_->comboBoxXbtWallet);
                     utxos = utxoReservationManager_->getAvailableXbtUTXOs(
                        replyData->xbtWallet->walletId(), purpose);
                  }
                  else {
                     utxos = utxoReservationManager_->getAvailableXbtUTXOs(
                        replyData->xbtWallet->walletId());
                  }
                  auto fixedUtxo = utxoReservationManager_->convertUtxoToPartialFixedInput(replyData->xbtWallet->walletId(), utxos);
                  inputsCb(fixedUtxo.inputs);
               }
            };

            if (qrn.side == bs::network::Side::Buy) {
               cbFee(0);
            } else {
               walletsManager_->estimatedFeePerByte(2, cbFee, this);
            }
         };
         // recv. address is always set automatically
         getAddress(qrn.quoteRequestId, recvWallet, AddressType::Recv, recvAddrCb);
         break;
      }

      default: {
         break;
      }
   }
}

void RFQDealerReply::updateWalletsList(int walletsFlags)
{
   auto oldWalletId = UiUtils::getSelectedWalletId(ui_->comboBoxXbtWallet);
   auto oldType = UiUtils::getSelectedWalletType(ui_->comboBoxXbtWallet);
   int defaultIndex = 0;
   if (walletsManager_) {
      defaultIndex = UiUtils::fillHDWalletsComboBox(ui_->comboBoxXbtWallet, walletsManager_, walletsFlags);
   }
   else {
      defaultIndex = UiUtils::fillHDWalletsComboBox(ui_->comboBoxXbtWallet, wallets_, walletsFlags);
   }
   int oldIndex = UiUtils::selectWalletInCombobox(ui_->comboBoxXbtWallet, oldWalletId, oldType);
   if (oldIndex < 0) {
      ui_->comboBoxXbtWallet->setCurrentIndex(defaultIndex);
   }
   walletSelected(ui_->comboBoxXbtWallet->currentIndex());
}

bool RFQDealerReply::isXbtSpend() const
{
   bool isXbtSpend = (currentQRN_.assetType == bs::network::Asset::PrivateMarket && currentQRN_.side == bs::network::Side::Sell) ||
      ((currentQRN_.assetType == bs::network::Asset::SpotXBT) && (currentQRN_.side == bs::network::Side::Buy));
   return isXbtSpend;
}

std::string RFQDealerReply::getSelectedXbtWalletId(ReplyType replyType) const
{
   std::string walletId;
   if (replyType == ReplyType::Manual) {
      walletId = ui_->comboBoxXbtWallet->currentData(UiUtils::WalletIdRole).toString().toStdString();
   }
   else {
      if (walletsManager_) {
         walletId = walletsManager_->getPrimaryWallet()->walletId();
      }  //new code doesn't support scripting in the GUI
   }
   return walletId;
}

void RFQDealerReply::onReservedUtxosChanged(const std::string &walletId, const std::vector<UTXO> &utxos)
{
   onTransactionDataChanged();
}

void RFQDealerReply::submitButtonClicked()
{
   const auto price = getPrice();
   if (!ui_->pushButtonSubmit->isEnabled() || price == 0) {
      return;
   }

   submitReply(currentQRN_, price, ReplyType::Manual);

   updateSubmitButton();
}

void RFQDealerReply::pullButtonClicked()
{
   if (currentQRN_.empty()) {
      return;
   }
   emit pullQuoteNotif(currentQRN_.settlementId, currentQRN_.quoteRequestId, currentQRN_.sessionToken);
   sentNotifs_.erase(currentQRN_.quoteRequestId);
   updateSubmitButton();
   refreshSettlementDetails();
}

bool RFQDealerReply::eventFilter(QObject *watched, QEvent *evt)
{
   if (evt->type() == QEvent::KeyPress) {
      auto keyID = static_cast<QKeyEvent *>(evt)->key();
      if ((keyID == Qt::Key_Return) || (keyID == Qt::Key_Enter)) {
         submitButtonClicked();
      }
   } else if ((evt->type() == QEvent::FocusIn) || (evt->type() == QEvent::FocusOut)) {
      auto activePriceWidget = getActivePriceWidget();
      if (activePriceWidget != nullptr) {
         autoUpdatePrices_ = !(activePriceWidget->hasFocus());
      } else {
         autoUpdatePrices_ = false;
      }
   }
   return QWidget::eventFilter(watched, evt);
}

void RFQDealerReply::showCoinControl()
{
   auto xbtWallet = getSelectedXbtWallet(ReplyType::Manual);
   if (!xbtWallet) {
      SPDLOG_LOGGER_ERROR(logger_, "can't find XBT wallet");
      return;
   }

   const auto &leaves = xbtWallet->getGroup(xbtWallet->getXBTGroupType())->getLeaves();
   std::vector<std::shared_ptr<bs::sync::Wallet>> wallets(leaves.begin(), leaves.end());
   ui_->toolButtonXBTInputsSend->setEnabled(false);

   // Need to release current reservation to be able select them back
   selectedXbtRes_.release();
   std::vector<UTXO> allUTXOs;
   if (!xbtWallet->canMixLeaves()) {
      auto purpose = UiUtils::getSelectedHwPurpose(ui_->comboBoxXbtWallet);
      allUTXOs = utxoReservationManager_->getAvailableXbtUTXOs(xbtWallet->walletId(), purpose);
   }
   else {
      allUTXOs = utxoReservationManager_->getAvailableXbtUTXOs(xbtWallet->walletId());
   }

   ui_->toolButtonXBTInputsSend->setEnabled(true);

   const bool useAutoSel = selectedXbtInputs_.empty();

   auto inputs = std::make_shared<SelectedTransactionInputs>(allUTXOs);

   // Set this to false is needed otherwise current selection would be cleared
   inputs->SetUseAutoSel(useAutoSel);
   for (const auto &utxo : selectedXbtInputs_) {
      inputs->SetUTXOSelection(utxo.getTxHash(), utxo.getTxOutIndex());
   }

   CoinControlDialog dialog(inputs, true, this);
   int rc = dialog.exec();
   if (rc != QDialog::Accepted) {
      return;
   }

   auto selectedInputs = dialog.selectedInputs();
   if (bs::UtxoReservation::instance()->containsReservedUTXO(selectedInputs)) {
      BSMessageBox(BSMessageBox::critical, tr("UTXO reservation failed"),
         tr("Some of selected UTXOs has been already reserved"), this).exec();
      showCoinControl();
      return;
   }

   selectedXbtInputs_.clear();
   for (const auto &selectedInput : selectedInputs) {
      selectedXbtInputs_.push_back(selectedInput);
   }

   if (!selectedXbtInputs_.empty()) {
      selectedXbtRes_ = utxoReservationManager_->makeNewReservation(selectedInputs);
   }

   updateSubmitButton();
}

void RFQDealerReply::validateGUI()
{
   updateSubmitButton();
}

void RFQDealerReply::onTransactionDataChanged()
{
   QMetaObject::invokeMethod(this, &RFQDealerReply::updateSubmitButton);
}

void RFQDealerReply::onMDUpdate(bs::network::Asset::Type, const QString &security, bs::network::MDFields mdFields)
{
   auto &mdInfo = mdInfo_[security.toStdString()];
   mdInfo.merge(bs::network::MDField::get(mdFields));
   if (autoUpdatePrices_ && (currentQRN_.security == security.toStdString())
      && (bestQPrices_.find(currentQRN_.quoteRequestId) == bestQPrices_.end())) {
      if (!qFuzzyIsNull(mdInfo.bidPrice)) {
         ui_->spinBoxBidPx->setValue(mdInfo.bidPrice);
      }
      if (!qFuzzyIsNull(mdInfo.askPrice)) {
         ui_->spinBoxOfferPx->setValue(mdInfo.askPrice);
      }
   }
}

void RFQDealerReply::onBestQuotePrice(const QString reqId, double price, bool own)
{
   bestQPrices_[reqId.toStdString()] = price;

   if (!currentQRN_.empty() && (currentQRN_.quoteRequestId == reqId.toStdString())) {
      if (autoUpdatePrices_) {
         auto priceWidget = getActivePriceWidget();
         if (priceWidget && !own) {
            double improvedPrice = price;
            const auto assetType = assetManager_->GetAssetTypeForSecurity(currentQRN_.security);
            if (assetType != bs::network::Asset::Type::Undefined) {
               const auto pip = std::pow(10, -UiUtils::GetPricePrecisionForAssetType(assetType));
               if (priceWidget == ui_->spinBoxBidPx) {
                  improvedPrice += pip;
               } else {
                  improvedPrice -= pip;
               }
            } else {
               logger_->error("[RFQDealerReply::onBestQuotePrice] could not get type for {}", currentQRN_.security);
            }
            priceWidget->setValue(improvedPrice);
         }
      }
   }

   updateSpinboxes();
}

void RFQDealerReply::onAQReply(const bs::network::QuoteReqNotification &qrn, double price)
{
   // Check assets first
   bool ok = true;
   if (qrn.assetType == bs::network::Asset::Type::SpotXBT) {
      if (qrn.side == bs::network::Side::Sell && qrn.product == bs::network::XbtCurrency) {
         CurrencyPair currencyPair(qrn.security);
         ok = assetManager_->checkBalance(currencyPair.ContraCurrency(qrn.product), qrn.quantity * price);
      }
      else if (qrn.side == bs::network::Side::Buy && qrn.product != bs::network::XbtCurrency) {
         ok = assetManager_->checkBalance(qrn.product, qrn.quantity);
      }
   }
   else if (qrn.assetType == bs::network::Asset::Type::SpotFX) {
      if (qrn.side == bs::network::Side::Sell) {
         auto quantity = qrn.quantity;
         CurrencyPair currencyPair(qrn.security);
         const auto contrCurrency = currencyPair.ContraCurrency(qrn.product);
         if (currencyPair.NumCurrency() == contrCurrency) {
            quantity /= price;
         }
         else {
            quantity *= price;
         }
         ok = assetManager_->checkBalance(currencyPair.ContraCurrency(qrn.product), quantity);
      }
      else {
         ok = assetManager_->checkBalance(qrn.product, qrn.quantity);
      }
   }

   if (!ok) {
      return;
   }

   submitReply(qrn, price, ReplyType::Script);
}

void RFQDealerReply::onHDLeafCreated(const std::string& ccName)
{
   if (product_ != ccName) {
      return;
   }

   auto ccLeaf = walletsManager_->getCCWallet(ccName);
   if (ccLeaf == nullptr) {
      logger_->error("[RFQDealerReply::onHDLeafCreated] CC wallet {} should exists"
                     , ccName);
      return;
   }

   updateUiWalletFor(currentQRN_);
   reset();
}

void RFQDealerReply::onCreateHDWalletError(const std::string& ccName, bs::error::ErrorCode result)
{
   if (product_ != ccName) {
      return;
   }

   BSMessageBox(BSMessageBox::critical, tr("Failed to create wallet")
      , tr("Failed to create wallet")
      , tr("%1 Wallet. Error: %2").arg(QString::fromStdString(product_)).arg(bs::error::ErrorCodeToString(result))).exec();
}

void RFQDealerReply::onCelerConnected()
{
   celerConnected_ = true;
   validateGUI();
}

void RFQDealerReply::onCelerDisconnected()
{
   logger_->info("Disabled auto-quoting due to Celer disconnection");
   celerConnected_ = false;
   validateGUI();
}

void RFQDealerReply::onAutoSignStateChanged()
{
   if (autoSignProvider_->autoSignState() == bs::error::ErrorCode::NoError) {
      ui_->comboBoxXbtWallet->setCurrentText(autoSignProvider_->getAutoSignWalletName());
   }
   ui_->comboBoxXbtWallet->setEnabled(autoSignProvider_->autoSignState() == bs::error::ErrorCode::AutoSignDisabled);
}

void bs::ui::RFQDealerReply::onQuoteCancelled(const std::string &quoteId)
{
   preparingCCRequest_.erase(quoteId);
}

void bs::ui::RFQDealerReply::onUTXOReservationChanged(const std::string& walletId)
{
   if (walletId.empty()) {
      updateBalanceLabel();
      return;
   }

   auto xbtWallet = getSelectedXbtWallet(ReplyType::Manual);
   if (xbtWallet && (walletId == xbtWallet->walletId() || xbtWallet->getLeaf(walletId))) {
      updateBalanceLabel();
   }
}

void bs::ui::RFQDealerReply::submit(double price, const std::shared_ptr<SubmitQuoteReplyData>& replyData)
{
   SPDLOG_LOGGER_DEBUG(logger_, "submitted quote reply on {}: {}"
      , replyData->qn.quoteRequestId, replyData->qn.price);
   sentNotifs_[replyData->qn.quoteRequestId] = price;
   submitQuoteNotifCb_(replyData);
   activeQuoteSubmits_.erase(replyData->qn.quoteRequestId);
   updateSubmitButton();
   refreshSettlementDetails();
}

void bs::ui::RFQDealerReply::reserveBestUtxoSetAndSubmit(double quantity, double price,
   const std::shared_ptr<SubmitQuoteReplyData>& replyData, ReplyType replyType)
{
   auto replyRFQWrapper = [rfqReply = QPointer<bs::ui::RFQDealerReply>(this),
      price, replyData, replyType] (std::vector<UTXO> utxos) {
      if (!rfqReply) {
         return;
      }

      if (utxos.empty()) {
         if (replyType == ReplyType::Manual) {
            replyData->fixedXbtInputs = rfqReply->selectedXbtInputs_;
            replyData->utxoRes = std::move(rfqReply->selectedXbtRes_);
         }

         rfqReply->submit(price, replyData);
         return;
      }

      if (replyType == ReplyType::Manual) {
         rfqReply->selectedXbtInputs_ = utxos;
      }

      replyData->utxoRes = rfqReply->utxoReservationManager_->makeNewReservation(utxos);
      replyData->fixedXbtInputs = std::move(utxos);

      rfqReply->submit(price, replyData);
   };

   if ((replyData->qn.side == bs::network::Side::Sell && replyData->qn.product != bs::network::XbtCurrency) ||
      (replyData->qn.side == bs::network::Side::Buy && replyData->qn.product == bs::network::XbtCurrency)) {
      replyRFQWrapper({});
      return; // Nothing to reserve
   }

   // We shouldn't recalculate better utxo set if that not first quote response
   // otherwise, we should chose best set if that wasn't done by user and this is not auto quoting script
   if (sentNotifs_.count(replyData->qn.quoteRequestId) || (!selectedXbtInputs_.empty() && replyType == ReplyType::Manual)) {
      replyRFQWrapper({});
      return; // already reserved by user
   }

   auto security = mdInfo_.find(replyData->qn.security);
   if (security == mdInfo_.end()) {
      // there is no MD data available so we really can't forecast
      replyRFQWrapper({});
      return;
   }

   BTCNumericTypes::satoshi_type xbtQuantity = 0;
   if (replyData->qn.side == bs::network::Side::Buy) {
      if (replyData->qn.assetType == bs::network::Asset::PrivateMarket) {
         xbtQuantity = XBTAmount(quantity * mdInfo_[replyData->qn.security].bidPrice).GetValue();
      }
      else if (replyData->qn.assetType == bs::network::Asset::SpotXBT) {
         xbtQuantity = XBTAmount(quantity / mdInfo_[replyData->qn.security].askPrice).GetValue();
      }
   }
   else {
      xbtQuantity = XBTAmount(quantity).GetValue();
   }
   xbtQuantity = static_cast<uint64_t>(xbtQuantity * tradeutils::reservationQuantityMultiplier());

   auto cbBestUtxoSet = [rfqReply = QPointer<bs::ui::RFQDealerReply>(this),
      replyRFQ = std::move(replyRFQWrapper)](std::vector<UTXO>&& utxos) {
      if (!rfqReply) {
         return;
      }

      replyRFQ(std::move(utxos));
   };

   // Check amount (required for AQ scripts)
   auto checkAmount = bs::UTXOReservationManager::CheckAmount::Enabled;

   if (!replyData->xbtWallet->canMixLeaves()) {
      auto purpose = UiUtils::getSelectedHwPurpose(ui_->comboBoxXbtWallet);
      utxoReservationManager_->getBestXbtUtxoSet(replyData->xbtWallet->walletId(), purpose,
         xbtQuantity, cbBestUtxoSet, true, checkAmount);
   }
   else {
      utxoReservationManager_->getBestXbtUtxoSet(replyData->xbtWallet->walletId(),
         xbtQuantity, cbBestUtxoSet, true, checkAmount);
   }


}

void bs::ui::RFQDealerReply::refreshSettlementDetails()
{
   if (currentQRN_.empty()) {
      ui_->groupBoxSettlementInputs->setEnabled(true);
      return;
   }

   ui_->groupBoxSettlementInputs->setEnabled(!sentNotifs_.count(currentQRN_.quoteRequestId));
}

void bs::ui::RFQDealerReply::updateSpinboxes()
{
   auto setSpinboxValue = [&](CustomDoubleSpinBox* spinBox, double value, double changeSign) {
      if (qFuzzyIsNull(value)) {
         spinBox->clear();
         return;
      }

      if (!spinBox->isEnabled()) {
         spinBox->setValue(value);
         return;
      }

      auto bestQuotePrice = bestQPrices_.find(currentQRN_.quoteRequestId);
      if (bestQuotePrice != bestQPrices_.end()) {
         spinBox->setValue(bestQuotePrice->second + changeSign * spinBox->singleStep());
      }
      else {
         spinBox->setValue(value);
      }
   };

   // The best quote response for buy orders should decrease price
   setSpinboxValue(ui_->spinBoxBidPx, indicBid_, 1.0);
   setSpinboxValue(ui_->spinBoxOfferPx, indicAsk_, -1.0);
}

void bs::ui::RFQDealerReply::updateBalanceLabel()
{
   QString totalBalance = kNoBalanceAvailable;

   if (isXbtSpend()) {
      totalBalance = tr("%1 %2")
         .arg(UiUtils::displayAmount(getXbtBalance().GetValueBitcoin()))
         .arg(QString::fromStdString(bs::network::XbtCurrency));
   } else if ((currentQRN_.side == bs::network::Side::Buy) && (currentQRN_.assetType == bs::network::Asset::PrivateMarket)) {
      totalBalance = tr("%1 %2")
         .arg(UiUtils::displayCCAmount(getPrivateMarketCoinBalance()))
         .arg(QString::fromStdString(baseProduct_));
   } else {
      if (assetManager_) {
         totalBalance = tr("%1 %2")
            .arg(UiUtils::displayCurrencyAmount(assetManager_->getBalance(product_)))
            .arg(QString::fromStdString(currentQRN_.side == bs::network::Side::Buy ? baseProduct_ : product_));
      }
      else {
         totalBalance = tr("%1 %2").arg(UiUtils::displayCurrencyAmount(balances_.at(product_)))
            .arg(QString::fromStdString(currentQRN_.side == bs::network::Side::Buy ? baseProduct_ : product_));
      }
   }

   ui_->labelBalanceValue->setText(totalBalance);
   ui_->cashBalanceLabel->setText(selectedXbtInputs_.empty() ? kAvailableBalance : kReservedBalance);
}

bs::XBTAmount RFQDealerReply::getXbtBalance() const
{
   const auto fixedInputs = selectedXbtInputs(ReplyType::Manual);
   if (!fixedInputs.empty()) {
      uint64_t sum = 0;
      for (const auto &utxo : fixedInputs) {
         sum += utxo.getValue();
      }
      return bs::XBTAmount(sum);
   }

   if (walletsManager_) {
      auto xbtWallet = getSelectedXbtWallet(ReplyType::Manual);
      if (!xbtWallet) {
         return {};
      }
      if (!xbtWallet->canMixLeaves()) {
         auto purpose = UiUtils::getSelectedHwPurpose(ui_->comboBoxXbtWallet);
         return bs::XBTAmount(utxoReservationManager_->getAvailableXbtUtxoSum(
            xbtWallet->walletId(), purpose));
      } else {
         return bs::XBTAmount(utxoReservationManager_->getAvailableXbtUtxoSum(
            xbtWallet->walletId()));
      }
   }
   else {
      const auto& xbtWalletId = getSelectedXbtWalletId(ReplyType::Manual);
      if (xbtWalletId.empty()) { // no wallet selected
         return {};
      }
      //TODO: distinguish between HW and SW wallets later
      double balance = 0;
      for (const auto& wallet : wallets_) {
         if (wallet.id == xbtWalletId) {
            for (const auto& group : wallet.groups) {
               switch (group.type) {
               case bs::hd::CoinType::Bitcoin_main:
               case bs::hd::CoinType::Bitcoin_test:
                  for (const auto& leaf : group.leaves) {
                     for (const auto& id : leaf.ids) {
                        balance += balances_.at(id);
                     }
                  }
                  break;
               default: break;
               }
            }
            break;
         }
      }
      return bs::XBTAmount{balance};
   }
}

BTCNumericTypes::balance_type bs::ui::RFQDealerReply::getPrivateMarketCoinBalance() const
{
   if (walletsManager_) {
      auto ccWallet = getCCWallet(currentQRN_.product);
      if (!ccWallet) {
         return 0;
      }
      return ccWallet->getSpendableBalance();
   }
   else {
      //TODO
      return 0;
   }
}
