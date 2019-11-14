#include "RFQDealerReply.h"
#include "ui_RFQDealerReply.h"

#include <spdlog/logger.h>

#include <chrono>

#include <QComboBox>
#include <QFileDialog>
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
#include "TxClasses.h"
#include "UiUtils.h"
#include "UserScriptRunner.h"
#include "Wallets/SyncHDWallet.h"
#include "Wallets/SyncWalletsManager.h"

namespace {
   const QString kNoBalanceAvailable = QLatin1String("-");

   constexpr auto kBuySortOrder = bs::core::wallet::OutputSortOrder{
      bs::core::wallet::OutputOrderType::Recipients,
      bs::core::wallet::OutputOrderType::PrevState,
      bs::core::wallet::OutputOrderType::Change
   };
   constexpr auto kSellSortOrder = bs::core::wallet::OutputSortOrder{
      bs::core::wallet::OutputOrderType::PrevState,
      bs::core::wallet::OutputOrderType::Recipients,
      bs::core::wallet::OutputOrderType::Change
   };
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
   connect(ui_->pushButtonAdvanced, &QPushButton::clicked, this, &RFQDealerReply::showCoinControl);

   connect(ui_->comboBoxWallet, qOverload<int>(&QComboBox::currentIndexChanged), this, &RFQDealerReply::walletSelected);
   connect(ui_->authenticationAddressComboBox, qOverload<int>(&QComboBox::currentIndexChanged), this, &RFQDealerReply::onAuthAddrChanged);

   ui_->responseTitle->hide();
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
   , const std::shared_ptr<AutoSignQuoteProvider> &autoSignQuoteProvider)
{
   logger_ = logger;
   assetManager_ = assetManager;
   quoteProvider_ = quoteProvider;
   authAddressManager_ = authAddressManager;
   appSettings_ = appSettings;
   signingContainer_ = container;
   armory_ = armory;
   connectionManager_ = connectionManager;
   autoSignQuoteProvider_ = autoSignQuoteProvider;

   connect(autoSignQuoteProvider_->autoQuoter(), &UserScriptRunner::sendQuote, this, &RFQDealerReply::onAQReply, Qt::QueuedConnection);
   connect(autoSignQuoteProvider_->autoQuoter(), &UserScriptRunner::pullQuoteNotif, this, &RFQDealerReply::pullQuoteNotif, Qt::QueuedConnection);

   connect(autoSignQuoteProvider_.get(), &AutoSignQuoteProvider::autoSignStateChanged, this, &RFQDealerReply::onAutoSignStateChanged, Qt::QueuedConnection);
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

   if (autoSignQuoteProvider_->autoQuoter()) {
      autoSignQuoteProvider_->autoQuoter()->setWalletsManager(walletsManager_);
   }

   auto updateAuthAddresses = [this] {
      UiUtils::fillAuthAddressesComboBox(ui_->authenticationAddressComboBox, authAddressManager_);
      onAuthAddrChanged(ui_->authenticationAddressComboBox->currentIndex());
   };
   updateAuthAddresses();
   connect(authAddressManager_.get(), &AuthAddressManager::VerifiedAddressListUpdated, this, updateAuthAddresses);
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
      indicBid_ = indicAsk_ = 0;
      setBalanceOk(true);
   }
   else {
      CurrencyPair cp(currentQRN_.security);
      baseProduct_ = cp.NumCurrency();
      product_ = cp.ContraCurrency(currentQRN_.product);

      const auto assetType = assetManager_->GetAssetTypeForSecurity(currentQRN_.security);
      if (assetType == bs::network::Asset::Type::Undefined) {
         logger_->error("[RFQDealerReply::reset] could not get asset type for {}", currentQRN_.security);
      }
      const auto priceDecimals = UiUtils::GetPricePrecisionForAssetType(assetType);
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
   }

   updateRespQuantity();
   updateSpinboxes();

   auto xbtWallet = getSelectedXbtWallet();
   selectedXbtInputs_ = xbtWallet ? std::make_shared<SelectedTransactionInputs>(xbtWallet, true, true) : nullptr;
}

void RFQDealerReply::quoteReqNotifStatusChanged(const bs::network::QuoteReqNotification &qrn)
{
   if (!QuoteProvider::isRepliableStatus(qrn.status)) {
      sentNotifs_.erase(qrn.quoteRequestId);
   }

   if (qrn.quoteRequestId == currentQRN_.quoteRequestId) {
      updateQuoteReqNotification(qrn);
   }
}

void RFQDealerReply::setQuoteReqNotification(const bs::network::QuoteReqNotification &qrn, double indicBid, double indicAsk)
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

   ui_->authenticationAddressLabel->setVisible(isXBT);
   ui_->authenticationAddressComboBox->setVisible(isXBT);
   ui_->widgetWallet->setVisible(isXBT || isPrivMkt);
   ui_->pushButtonAdvanced->setVisible(isXBT && (qrn.side == bs::network::Side::Buy));
   ui_->labelWallet->setText(qrn.side == bs::network::Side::Buy ? tr("Payment Wallet") : tr("Receiving Wallet"));

   dealerSellXBT_ = (isXBT || isPrivMkt) && ((qrn.product == bs::network::XbtCurrency) != (qrn.side == bs::network::Side::Sell));

   updateUiWalletFor(qrn);

   if (qrnChanged) {
      reset();
   }

   if (qrn.assetType == bs::network::Asset::SpotFX ||
      qrn.assetType == bs::network::Asset::Undefined) {
         ui_->responseTitle->hide();
   } else {
      ui_->responseTitle->show();
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

void RFQDealerReply::getRecvAddress(const std::shared_ptr<bs::sync::Wallet> &wallet, std::function<void(bs::Address)> cb) const
{
   if (!wallet) {
      cb({});
      return;
   }

   auto cbWrap = [wallet, cb = std::move(cb)](const bs::Address &addr) {
      if (wallet->type() != bs::core::wallet::Type::ColorCoin) {
         wallet->setAddressComment(addr, bs::sync::wallet::Comment::toString(bs::sync::wallet::Comment::SettlementPayOut));
      }
      cb(addr);
   };
   wallet->getNewIntAddress(cbWrap);
   //curWallet_->RegisterWallet();  //TODO: invoke at address callback
}

void RFQDealerReply::updateUiWalletFor(const bs::network::QuoteReqNotification &qrn)
{
   if (armory_->state() != ArmoryState::Ready) {
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

      const bool skipWatchingOnly = (qrn.side == bs::network::Side::Sell);
      updateWalletsList(skipWatchingOnly);
   }
   else if (qrn.assetType == bs::network::Asset::SpotXBT) {
      const bool skipWatchingOnly = (currentQRN_.side == bs::network::Side::Buy);
      updateWalletsList(skipWatchingOnly);
   }
}

void RFQDealerReply::priceChanged()
{
   updateRespQuantity();
   updateSubmitButton();
}

void RFQDealerReply::onAuthAddrChanged(int index)
{
   authAddr_ = authAddressManager_->GetAddress(authAddressManager_->FromVerifiedIndex(index));
   authKey_.clear();

   if (authAddr_.isNull()) {
      return;
   }
   const auto settlLeaf = authAddressManager_->getSettlementLeaf(authAddr_);

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
   updateBalanceLabel();
   bool isQRNRepliable = (!currentQRN_.empty() && QuoteProvider::isRepliableStatus(currentQRN_.status));
   if ((currentQRN_.assetType != bs::network::Asset::SpotFX)
      && (!signingContainer_ || signingContainer_->isOffline())) {
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

   if (!assetManager_) {
      ui_->pushButtonSubmit->setEnabled(false);
      return;
   }

   const bool isBalanceOk = checkBalance();
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

   // FIXME: Balance check should account for fee?

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

std::shared_ptr<bs::sync::Wallet> RFQDealerReply::getSelectedXbtWallet() const
{
   if (!walletsManager_) {
      return nullptr;
   }
   return walletsManager_->getWalletById(ui_->comboBoxWallet->currentData(UiUtils::WalletIdRole).toString().toStdString());
}

bs::Address RFQDealerReply::selectedAuthAddress() const
{
   return authAddr_;
}

std::vector<UTXO> RFQDealerReply::selectedXbtInputs() const
{
   if (!selectedXbtInputs_ || selectedXbtInputs_->UseAutoSel()) {
      return {};
   }
   return selectedXbtInputs_->GetSelectedTransactions();
}

void RFQDealerReply::setSubmitQuoteNotifCb(RFQDealerReply::SubmitQuoteNotifCb cb)
{
   submitQuoteNotifCb_ = std::move(cb);
}

void RFQDealerReply::submitReply(const bs::network::QuoteReqNotification &qrn
   , double price, SubmitCb cb, const std::shared_ptr<bs::sync::Wallet> &xbtWallet)
{
   if (qFuzzyIsNull(price)) {
      cb({}, {});
      return;
   }
   const auto itQN = sentNotifs_.find(qrn.quoteRequestId);
   if ((itQN != sentNotifs_.end()) && (itQN->second == price)) {
      cb({}, {});
      return;
   }

   auto qn = std::make_shared<bs::network::QuoteNotification>(qrn, authKey_, price, "");

   switch (qrn.assetType) {
      case bs::network::Asset::SpotFX: {
         cb(*qn, {});
         break;
      }

      case bs::network::Asset::SpotXBT: {
         cb(*qn, {});
         break;
      }

      case bs::network::Asset::PrivateMarket: {
         auto ccWallet = getCCWallet(qrn);
         if (!ccWallet) {
            SPDLOG_LOGGER_ERROR(logger_, "can't find required CC wallet");
            cb({}, {});
            return;
         }

         const bool isSpendCC = qrn.side == bs::network::Side::Buy;
         uint64_t spendVal;
         if (isSpendCC) {
            spendVal = qrn.quantity * assetManager_->getCCLotSize(qrn.product);
         } else {
            uint64_t priceQty = std::ceil(price * qrn.quantity * 1000000.0);   // ugly hack to avoid rounding errors
            priceQty *= 100;        // how to reproduce: sell 1 CC RFQ - reply to it with .012302 - without the hack
            spendVal = priceQty;   // the result in spendVal (after trivial multiply) will be 1230199 (on Windows)
         }

         const auto &spendWallet = isSpendCC ? ccWallet : xbtWallet;
         const auto &recvWallet = isSpendCC ? xbtWallet : ccWallet;
         // For CC search for exact amount (as we have no need for change).
         // For XBT request all available inputs as we don't know fee yet (createPartialTXRequest will use correct inputs if fee is set)
         const uint64_t requestUtxoVal = isSpendCC ? spendVal : std::numeric_limits<uint64_t>::max();

         auto recvAddrCb = [this, cb, qn, qrn, spendWallet, spendVal, isSpendCC, requestUtxoVal](const bs::Address &addr) {
            qn->receiptAddress = addr.display();
            qn->reqAuthKey = qrn.requestorRecvAddress;

            const auto &cbFee = [this, qrn, spendVal, spendWallet, isSpendCC, cb, qn, requestUtxoVal](float feePerByte) {
               auto inputsCb = [this, qrn, feePerByte, qn, spendVal, spendWallet, isSpendCC, cb](const std::vector<UTXO> &inputs) {
                  QMetaObject::invokeMethod(this, [this, feePerByte, qrn, qn, spendVal, spendWallet, isSpendCC, cb, inputs] {
                     const auto &cbChangeAddr = [this, feePerByte, qrn, qn, spendVal, spendWallet, cb, inputs]
                        (const bs::Address &changeAddress)
                     {
                        try {
                           const auto recipient = bs::Address::fromAddressString(qrn.requestorRecvAddress).getRecipient(bs::XBTAmount{ spendVal });

                           logger_->debug("[cbFee] {} input[s], fpb={}, recip={}, prevPart={}", inputs.size(), feePerByte
                              , bs::Address::fromAddressString(qrn.requestorRecvAddress).display(), qrn.requestorAuthPublicKey);
                           const auto outSortOrder = (qrn.side == bs::network::Side::Buy) ? kBuySortOrder : kSellSortOrder;
                           const auto txReq = spendWallet->createPartialTXRequest(spendVal, inputs, changeAddress, feePerByte
                              , { recipient }, outSortOrder, BinaryData::CreateFromHex(qrn.requestorAuthPublicKey), false);
                           qn->transactionData = txReq.serializeState().toHexStr();
                           auto utxoRes = bs::UtxoReservationToken::makeNewReservation(logger_, txReq, qn->quoteRequestId);
                           cb(*qn, std::move(utxoRes));
                        } catch (const std::exception &e) {
                           logger_->error("[RFQDealerReply::submit] error creating own unsigned half: {}", e.what());
                           cb({}, {});
                           return;
                        }
                     };

                     if (isSpendCC) {
                        uint64_t inputsVal = 0;
                        for (const auto &input : inputs) {
                           inputsVal += input.getValue();
                        }
                        if (inputsVal == spendVal) {
                           cbChangeAddr({});
                           return;
                        }
                     }
                     spendWallet->getNewChangeAddress(cbChangeAddr);
                  });
               };
               spendWallet->getSpendableTxOutList(inputsCb, requestUtxoVal);
            };

            if (qrn.side == bs::network::Side::Buy) {
               cbFee(0);
            } else {
               walletsManager_->estimatedFeePerByte(2, cbFee, this);
            }
         };
         getRecvAddress(recvWallet, recvAddrCb);
         break;
      }
   }
}

void RFQDealerReply::updateWalletsList(bool skipWatchingOnly)
{
   auto oldWalletId = ui_->comboBoxWallet->currentData(UiUtils::WalletIdRole).toString().toStdString();
   int defaultIndex = UiUtils::fillWalletsComboBox(ui_->comboBoxWallet, walletsManager_, skipWatchingOnly);
   int oldIndex = UiUtils::selectWalletInCombobox(ui_->comboBoxWallet, oldWalletId);
   if (oldIndex < 0) {
      ui_->comboBoxWallet->setCurrentIndex(defaultIndex);
   }
   walletSelected(ui_->comboBoxWallet->currentIndex());
}

bool RFQDealerReply::isXbtSpend() const
{
   bool isXbtSpend = (currentQRN_.assetType == bs::network::Asset::PrivateMarket && currentQRN_.side == bs::network::Side::Sell) ||
      ((currentQRN_.assetType == bs::network::Asset::SpotXBT) && (currentQRN_.side == bs::network::Side::Buy));
   return isXbtSpend;
}

void RFQDealerReply::onReservedUtxosChanged(const std::string &walletId, const std::vector<UTXO> &utxos)
{
   onTransactionDataChanged();
}

void RFQDealerReply::submitButtonClicked()
{
   const auto price = getPrice();
   if (!ui_->pushButtonSubmit->isEnabled() || qFuzzyIsNull(price)) {
      return;
   }

   const auto &cbSubmit = [this, price](bs::network::QuoteNotification qn, bs::UtxoReservationToken utxoRes) {
      if (!qn.quoteRequestId.empty()) {
         logger_->debug("Submitted quote reply on {}: {}/{}", currentQRN_.quoteRequestId, qn.bidPx, qn.offerPx);
         sentNotifs_[qn.quoteRequestId] = price;
         submitQuoteNotifCb_(std::move(qn), std::move(utxoRes));
      }
   };
   submitReply(currentQRN_, price, cbSubmit, getSelectedXbtWallet());
   updateSubmitButton();
}

void RFQDealerReply::pullButtonClicked()
{
   if (currentQRN_.empty()) {
      return;
   }
   emit pullQuoteNotif(QString::fromStdString(currentQRN_.quoteRequestId), QString::fromStdString(currentQRN_.sessionToken));
   sentNotifs_.erase(currentQRN_.quoteRequestId);
   updateSubmitButton();
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
   if (selectedXbtInputs_) {
      int rc = CoinControlDialog(selectedXbtInputs_, true, this).exec();
      if (rc == QDialog::Accepted) {
         updateSubmitButton();
      }
   }
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
   const double bid = bs::network::MDField::get(mdFields, bs::network::MDField::PriceBid).value;
   const double ask = bs::network::MDField::get(mdFields, bs::network::MDField::PriceOffer).value;
   const double last = bs::network::MDField::get(mdFields, bs::network::MDField::PriceLast).value;

   auto &mdInfo = mdInfo_[security.toStdString()];
   if (bid > 0) {
      mdInfo.bidPrice = bid;
   }
   if (ask > 0) {
      mdInfo.askPrice = ask;
   }
   if (last > 0) {
      mdInfo.lastPrice = last;
   }

   if (autoUpdatePrices_ && (currentQRN_.security == security.toStdString())
      && (bestQPrices_.find(currentQRN_.quoteRequestId) == bestQPrices_.end())) {
      if (!qFuzzyIsNull(bid)) {
         ui_->spinBoxBidPx->setValue(bid);
      }
      if (!qFuzzyIsNull(ask)) {
         ui_->spinBoxOfferPx->setValue(ask);
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
   QMetaObject::invokeMethod(this, [this, qrn, price] {
      const auto &cbSubmit = [this, qrn, price](bs::network::QuoteNotification qn, bs::UtxoReservationToken utxoRes) {
         if (!qn.quoteRequestId.empty()) {
            logger_->debug("Submitted AQ reply on {}: {}/{}", qrn.quoteRequestId, qn.bidPx, qn.offerPx);
            // Store AQ too so it's possible to pull it later (and to disable submit button)
            sentNotifs_[qn.quoteRequestId] = price;
            submitQuoteNotifCb_(std::move(qn), std::move(utxoRes));
         }
      };

      submitReply(qrn, price, cbSubmit, nullptr);
   });
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
   if (autoSignQuoteProvider_->autoSignState() == bs::error::ErrorCode::NoError) {
      ui_->comboBoxWallet->setCurrentText(autoSignQuoteProvider_->getAutoSignWalletName());
   }
   ui_->comboBoxWallet->setEnabled(autoSignQuoteProvider_->autoSignState() == bs::error::ErrorCode::AutoSignDisabled);
}

void bs::ui::RFQDealerReply::updateSpinboxes()
{
   auto setSpinboxValue = [&](CustomDoubleSpinBox* spinBox, double value) {
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
         spinBox->setValue(bestQuotePrice->second + spinBox->singleStep());
      }
      else {
         spinBox->setValue(value);
      }
   };

   setSpinboxValue(ui_->spinBoxBidPx, indicBid_);
   setSpinboxValue(ui_->spinBoxOfferPx, indicAsk_);
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
   }

   ui_->labelBalanceValue->setText(totalBalance);
}

bs::XBTAmount RFQDealerReply::getXbtBalance() const
{
   const auto fixedInputs = selectedXbtInputs();
   if (!fixedInputs.empty()) {
      uint64_t sum = 0;
      for (const auto &utxo : fixedInputs) {
         sum += utxo.getValue();
      }
      return bs::XBTAmount(sum);
   }

   auto xbtWallet = getSelectedXbtWallet();
   if (!xbtWallet) {
      return {};
   }

   return bs::XBTAmount(xbtWallet->getSpendableBalance());
}

BTCNumericTypes::balance_type bs::ui::RFQDealerReply::getPrivateMarketCoinBalance() const
{
   auto ccWallet = getCCWallet(currentQRN_.product);
   if (!ccWallet) {
      return 0;
   }
   return ccWallet->getSpendableBalance();
}
