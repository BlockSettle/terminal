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
#include "CustomComboBox.h"
#include "FastLock.h"
#include "QuoteProvider.h"
#include "SelectedTransactionInputs.h"
#include "SignContainer.h"
#include "TransactionData.h"
#include "TxClasses.h"
#include "UiUtils.h"
#include "UserScriptRunner.h"
#include "UtxoReserveAdapters.h"
#include "Wallets/SyncHDWallet.h"
#include "Wallets/SyncWalletsManager.h"

namespace {
   const QString noBalanceAvailable = QLatin1String("-");
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

RFQDealerReply::~RFQDealerReply()
{
   bs::UtxoReservation::delAdapter(dealerUtxoAdapter_);
}

void RFQDealerReply::init(const std::shared_ptr<spdlog::logger> logger
   , const std::shared_ptr<AuthAddressManager> &authAddressManager
   , const std::shared_ptr<AssetManager>& assetManager
   , const std::shared_ptr<QuoteProvider>& quoteProvider
   , const std::shared_ptr<ApplicationSettings> &appSettings
   , const std::shared_ptr<ConnectionManager> &connectionManager
   , const std::shared_ptr<SignContainer> &container
   , const std::shared_ptr<ArmoryConnection> &armory
   , const std::shared_ptr<bs::DealerUtxoResAdapter> &dealerUtxoAdapter
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
   dealerUtxoAdapter_ = dealerUtxoAdapter;
   autoSignQuoteProvider_ = autoSignQuoteProvider;

   connect(quoteProvider_.get(), &QuoteProvider::orderUpdated, dealerUtxoAdapter_.get(), &bs::OrderUtxoResAdapter::onOrder);
   connect(quoteProvider_.get(), &QuoteProvider::orderUpdated, this, &RFQDealerReply::onOrderUpdated);
   connect(dealerUtxoAdapter_.get(), &bs::OrderUtxoResAdapter::reservedUtxosChanged, this, &RFQDealerReply::onReservedUtxosChanged, Qt::QueuedConnection);

   connect(autoSignQuoteProvider_->autoQuoter(), &UserScriptRunner::sendQuote, this, &RFQDealerReply::onAQReply, Qt::QueuedConnection);
   connect(autoSignQuoteProvider_->autoQuoter(), &UserScriptRunner::pullQuoteNotif, this, &RFQDealerReply::pullQuoteNotif, Qt::QueuedConnection);

   connect(autoSignQuoteProvider_.get(), &AutoSignQuoteProvider::autoSignStateChanged, this, &RFQDealerReply::onAutoSignStateChanged, Qt::QueuedConnection);

   UtxoReservation::addAdapter(dealerUtxoAdapter_);
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

      transactionData_ = nullptr;
      if (currentQRN_.assetType != bs::network::Asset::SpotFX) {
         transactionData_ = std::make_shared<TransactionData>([this]() { onTransactionDataChanged(); }
            , logger_, true, true);
         if (walletsManager_ != nullptr) {
            const auto &cbFee = [this](float feePerByte) {
               transactionData_->setFeePerByte(feePerByte);
            };
            walletsManager_->estimatedFeePerByte(2, cbFee, this);
         }

         if (currentQRN_.assetType == bs::network::Asset::SpotXBT) {
            transactionData_->setWallet(getSelectedXbtWallet(), armory_->topBlock());
         } else if (currentQRN_.assetType == bs::network::Asset::PrivateMarket) {
            std::shared_ptr<bs::sync::Wallet> wallet;
            if (currentQRN_.side == bs::network::Side::Buy) {
               wallet = getCCWallet(currentQRN_.product);
            }
            else {
               wallet = getSelectedXbtWallet();
            }
            if (wallet && (!ccCoinSel_ || (ccCoinSel_->GetWallet() != wallet))) {
               ccCoinSel_ = std::make_shared<SelectedTransactionInputs>(wallet, true, true);
            }
            transactionData_->setSigningWallet(wallet);
            transactionData_->setWallet(wallet, armory_->topBlock());
         }
      }

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

   if ((currentQRN_.side == bs::network::Side::Buy) != (product_ == baseProduct_)) {
      const auto amount = getAmount();
      if ((currentQRN_.assetType == bs::network::Asset::SpotXBT) && transactionData_) {
         return (amount <= (transactionData_->GetTransactionSummary().availableBalance
            - transactionData_->GetTransactionSummary().totalFee / BTCNumericTypes::BalanceDivider));
      }
      else if ((currentQRN_.assetType == bs::network::Asset::PrivateMarket) && ccCoinSel_) {
         return (amount <= getPrivateMarketCoinBalance());
      }
      const auto product = (product_ == baseProduct_) ? product_ : currentQRN_.product;
      return assetManager_->checkBalance(product, amount);
   }
   else if ((currentQRN_.side == bs::network::Side::Buy) && (product_ == baseProduct_)) {
      return assetManager_->checkBalance(currentQRN_.product, currentQRN_.quantity);
   }

   if ((currentQRN_.assetType == bs::network::Asset::PrivateMarket) && transactionData_) {
      return (currentQRN_.quantity * getPrice() <= transactionData_->GetTransactionSummary().availableBalance
         - transactionData_->GetTransactionSummary().totalFee / BTCNumericTypes::BalanceDivider);
   }

   const double value = getValue();
   if (qFuzzyIsNull(value)) {
      return true;
   }
   const bool isXbt = (currentQRN_.assetType == bs::network::Asset::PrivateMarket) ||
      ((currentQRN_.assetType == bs::network::Asset::SpotXBT) && (product_ == baseProduct_));
   if (isXbt && transactionData_) {
      return (value <= (transactionData_->GetTransactionSummary().availableBalance
         - transactionData_->GetTransactionSummary().totalFee / BTCNumericTypes::BalanceDivider));
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

void RFQDealerReply::submitReply(const std::shared_ptr<TransactionData> transData
   , const bs::network::QuoteReqNotification &qrn, double price
   , std::function<void(bs::network::QuoteNotification)> cb
   , const std::shared_ptr<bs::sync::Wallet> &xbtWallet)
{
   if (qFuzzyIsNull(price)) {
      cb({});
      return;
   }
   const auto itQN = sentNotifs_.find(qrn.quoteRequestId);
   if ((itQN != sentNotifs_.end()) && (itQN->second == price)) {
      cb({});
      return;
   }
   bool isBid = (qrn.side == bs::network::Side::Buy);

   const auto &lbdQuoteNotif = [this, cb, qrn, price, transData, xbtWallet](const std::string &txData) {
      logger_->debug("[RFQDealerReply::submitReply] txData={}", txData);
      auto qn = std::make_shared<bs::network::QuoteNotification>(qrn, authKey_, price, txData);

      auto addrCb = [this, cb, qn, qrn, price, transData, xbtWallet](const bs::Address &addr) {
         qn->receiptAddress = addr.display();
         qn->reqAuthKey = qrn.requestorRecvAddress;

         auto wallet = transData->getSigningWallet();
         auto spendVal = std::make_shared<uint64_t>(0);

         const auto &cbFee = [this, qrn, transData, spendVal, wallet, cb, qn](float feePerByte) {
            const auto recipient = bs::Address(qrn.requestorRecvAddress).getRecipient(bs::XBTAmount{ *spendVal });
            std::vector<UTXO> inputs = dealerUtxoAdapter_->get(qn->quoteRequestId);
            if (inputs.empty() && ccCoinSel_) {
               inputs = ccCoinSel_->GetSelectedTransactions();
               if (inputs.empty()) {
                  logger_->error("[RFQDealerReply::submit] no suitable inputs for CC sell");
                  cb({});
                  return;
               }
            }

            const auto &cbAddr = [this, inputs, feePerByte, qrn, qn, spendVal, wallet, recipient, transData, cb](const bs::Address &changeAddress) {
               try {
                  logger_->debug("[cbFee] {} input[s], fpb={}, recip={}, prevPart={}", inputs.size(), feePerByte
                     , bs::Address(qrn.requestorRecvAddress).display(), qrn.requestorAuthPublicKey);
                  try {
                     Signer signer;
                     signer.deserializeState(BinaryData::CreateFromHex(qrn.requestorAuthPublicKey));
                     logger_->debug("[cbFee] deserialized state");
                  } catch (const std::exception &e) {
                     logger_->error("[cbFee] state deser failed: {}", e.what());
                  }
                  const auto txReq = wallet->createPartialTXRequest(*spendVal, inputs, changeAddress, feePerByte
                     , { recipient }, BinaryData::CreateFromHex(qrn.requestorAuthPublicKey));
                  qn->transactionData = txReq.serializeState().toHexStr();
                  dealerUtxoAdapter_->reserve(txReq, qn->quoteRequestId);
               } catch (const std::exception &e) {
                  logger_->error("[RFQDealerReply::submit] error creating own unsigned half: {}", e.what());
                  cb({});
                  return;
               }
               if (!transData->getSigningWallet()) {
                  transData->setSigningWallet(wallet);
               }
               cb(*qn);
            };
            wallet->getNewChangeAddress(cbAddr);
         };

         if (qrn.side == bs::network::Side::Buy) {
            if (!wallet) {
               wallet = getCCWallet(qrn.product);
            }
            *spendVal = qrn.quantity * assetManager_->getCCLotSize(qrn.product);
            cbFee(0);
            return;
         } else {
            *spendVal = qrn.quantity * price * BTCNumericTypes::BalanceDivider;
            walletsManager_->estimatedFeePerByte(2, cbFee, this);
            return;
         }
      };

      if (qrn.assetType == bs::network::Asset::PrivateMarket) {
         if (qrn.side == bs::network::Side::Sell) {
            getRecvAddress(getCCWallet(qrn), addrCb);
         } else {
            getRecvAddress(xbtWallet, addrCb);
         }
      }
      cb(*qn);
   };

   if ((qrn.assetType == bs::network::Asset::SpotXBT) && authAddressManager_ && transData) {
      if (authKey_.empty()) {
         logger_->error("[RFQDealerReply::submit] empty auth key");
         cb({});
         return;
      }
      logger_->debug("[RFQDealerReply::submit] using wallet {}", transData->getWallet()->name());

      const bool reversed = (qrn.product != bs::network::XbtCurrency);
      if (reversed) {
         isBid = !isBid;
      }
      const double quantity = reversed ? qrn.quantity / price : qrn.quantity;

      const auto priWallet = walletsManager_->getPrimaryWallet();
      const auto settlementId = BinaryData::CreateFromHex(qrn.settlementId);
      const auto group = priWallet->getGroup(bs::hd::BlockSettle_Settlement);
      std::shared_ptr<bs::sync::hd::SettlementLeaf> settlLeaf;
      if (group) {
         const auto settlGroup = std::dynamic_pointer_cast<bs::sync::hd::SettlementGroup>(group);
         if (settlGroup) {
            settlLeaf = settlGroup->getLeaf(authAddr_);
         }
         else {
            logger_->error("[RFQDealerReply::submit] failed to get settlement group");
         }
      }
      if (!settlLeaf) {
         logger_->error("[RFQDealerReply::submit] failed to get settlement leaf for {}", authAddr_.display());
         cb({});
         return;
      }

      if (isBid) {
         const auto &lbdUnsignedTx = [this, qrn, transData, lbdQuoteNotif, cb] {
            try {
               if (transData->IsTransactionValid()) {
                  auto unsignedTxReqCb = [this, transData, qrn, lbdQuoteNotif](const bs::core::wallet::TXSignRequest &unsignedTxReq) {
                     const auto cbPreimage = [this, unsignedTxReq, qrn, lbdQuoteNotif]
                        (const std::map<bs::Address, BinaryData> &preimages)
                     {
                        const auto resolver = bs::sync::WalletsManager::getPublicResolver(preimages);
                        dealerUtxoAdapter_->reserve(unsignedTxReq, qrn.settlementId);

                        const auto txData = unsignedTxReq.txId(resolver).toHexStr();
                        lbdQuoteNotif(txData);
                     };
                     const auto addrMapping = walletsManager_->getAddressToWalletsMapping(transData->inputs());
                     signingContainer_->getAddressPreimage(addrMapping, cbPreimage);
                  };
                  if (transData->GetTransactionSummary().hasChange) {
                     auto cbAddr = [unsignedTxReqCb, transData](const bs::Address &addr) {
                        transData->getWallet()->setAddressComment(addr, bs::sync::wallet::Comment::toString(
                           bs::sync::wallet::Comment::ChangeAddress));
                        auto unsignedTxReq = transData->createUnsignedTransaction(false, addr);
                        unsignedTxReqCb(unsignedTxReq);
                     };
                     transData->getWallet()->getNewChangeAddress(cbAddr);
                  } else {
                     auto unsignedTxReq = transData->createUnsignedTransaction();
                     unsignedTxReqCb(unsignedTxReq);
                  }
               } else {
                  logger_->warn("[RFQDealerReply::submit] pay-in transaction is invalid!");
               }
            }
            catch (const std::exception &e) {
               logger_->error("[RFQDealerReply::submit] failed to create pay-in transaction: {}", e.what());
            }
            cb({});
         };

         if ((payInRecipId_ == UINT_MAX) || !transData->GetRecipientsCount()) {
            const std::string &comment = std::string(bs::network::Side::toString(bs::network::Side::invert(qrn.side)))
               + " " + qrn.security + " @ " + std::to_string(price);
            const auto &cbSettlAddr = [this, transData, quantity, lbdUnsignedTx](const bs::Address &addr) {
               payInRecipId_ = transData->RegisterNewRecipient();
               transData->UpdateRecipientAmount(payInRecipId_, quantity);
               if (!transData->UpdateRecipientAddress(payInRecipId_, addr)) {
                  logger_->warn("[RFQDealerReply::submit] Failed to update address for recipient {}", payInRecipId_);
               }
               //TODO: set comment if needed
               lbdUnsignedTx();
            };
            const auto cpAuthPubKey = BinaryData::CreateFromHex(qrn.requestorAuthPublicKey);
            const auto &cbSetSettlId = [priWallet, settlementId, cpAuthPubKey, cbSettlAddr](bool result) {
               if (!result) {
                  return;
               }
               priWallet->getSettlementPayinAddress(settlementId, cpAuthPubKey, cbSettlAddr, false);
            };
            settlLeaf->setSettlementID(settlementId, cbSetSettlId);
         }
         else {
            transData->UpdateRecipientAmount(payInRecipId_, quantity);
            lbdUnsignedTx();
         }
         return;
      } else {
         settlLeaf->setSettlementID(settlementId, [](bool) {});
         auto addrCb = [transData](const bs::Address &addr) {
            transData->SetFallbackRecvAddress(addr);
         };
         getRecvAddress(xbtWallet, addrCb);
      }
   }
   lbdQuoteNotif({});
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

void RFQDealerReply::onReservedUtxosChanged(const std::string &walletId, const std::vector<UTXO> &utxos)
{
   if (ccCoinSel_ && (ccCoinSel_->GetWallet()->walletId() == walletId)) {
      ccCoinSel_->Reload(utxos);
   }
   if (transactionData_ && transactionData_->getWallet() && (transactionData_->getWallet()->walletId() == walletId)) {
      transactionData_->ReloadSelection(utxos);
   }
   onTransactionDataChanged();
}

void RFQDealerReply::submitButtonClicked()
{
   const auto price = getPrice();
   if (!ui_->pushButtonSubmit->isEnabled() || qFuzzyIsNull(price)) {
      return;
   }

   const auto &cbSubmit = [this, price](bs::network::QuoteNotification qn) {
      if (!qn.quoteRequestId.empty()) {
         logger_->debug("Submitted quote reply on {}: {}/{}", currentQRN_.quoteRequestId, qn.bidPx, qn.offerPx);
         sentNotifs_[qn.quoteRequestId] = price;
         emit submitQuoteNotif(qn);
      }
   };
   submitReply(transactionData_, currentQRN_, price, cbSubmit, getSelectedXbtWallet());
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
   if (currentQRN_.assetType == bs::network::Asset::PrivateMarket) {
      CoinControlDialog(ccCoinSel_, true, this).exec();
   } else {
      CoinControlDialog(transactionData_->getSelectedInputs(), true, this).exec();
   }
}

std::shared_ptr<TransactionData> RFQDealerReply::getTransactionData(const std::string &reqId) const
{
   if ((reqId == currentQRN_.quoteRequestId) && transactionData_) {
      return transactionData_;
   }

   if (autoSignQuoteProvider_->autoQuoter()) {
      return autoSignQuoteProvider_->autoQuoter()->getTransactionData(reqId);
   } else {
      return nullptr;
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

void RFQDealerReply::onOrderUpdated(const bs::network::Order &order)
{
   if ((order.assetType == bs::network::Asset::PrivateMarket) && (order.status == bs::network::Order::Failed)) {
      const auto &quoteReqId = quoteProvider_->getQuoteReqId(order.quoteId);
      if (!quoteReqId.empty()) {
         dealerUtxoAdapter_->unreserve(quoteReqId);
      }
   }
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
   const auto &cbSubmit = [this, qrn, price](const bs::network::QuoteNotification &qn) {
      if (!qn.quoteRequestId.empty()) {
         logger_->debug("Submitted AQ reply on {}: {}/{}", qrn.quoteRequestId, qn.bidPx, qn.offerPx);
         QMetaObject::invokeMethod(this, [this, qn, price] {
            // Store AQ too so it's possible to pull it later (and to disable submit button)
            sentNotifs_[qn.quoteRequestId] = price;
            emit submitQuoteNotif(qn);
         });
      }
   };

   std::shared_ptr<TransactionData> transData;

   if (qrn.assetType == bs::network::Asset::SpotFX) {
      submitReply(transData, qrn, price, cbSubmit, nullptr);
   }

   auto wallet = walletsManager_->getDefaultWallet();
   transData = std::make_shared<TransactionData>(TransactionData::onTransactionChanged{}, logger_, true, true);
   transData->disableTransactionUpdate();
   transData->setWallet(wallet, armory_->topBlock());

   if (qrn.assetType == bs::network::Asset::PrivateMarket) {
      const auto &cc = qrn.product;
      const auto& ccWallet = getCCWallet(cc);
      if (qrn.side == bs::network::Side::Buy) {
         transData->setSigningWallet(ccWallet);
      } else {
         if (!ccWallet) {
            autoSignQuoteProvider_->deinitAQ();
            BSMessageBox(BSMessageBox::critical, tr("Auto Quoting")
               , tr("No wallet created for %1 - auto-quoting disabled").arg(QString::fromStdString(cc))
            ).exec();
            return;
         }
         transData->setSigningWallet(wallet);
      }
   }

   const auto txUpdated = [this, qrn, price, cbSubmit, transData]()
   {
      logger_->debug("[RFQDealerReply::onAQReply TX CB] : tx updated for {} - {}"
         , qrn.quoteRequestId, (transData->InputsLoadedFromArmory() ? "inputs loaded" : "inputs not loaded"));

      if (transData->InputsLoadedFromArmory()) {
         autoSignQuoteProvider_->autoQuoter()->setTxData(qrn.quoteRequestId, transData);
         // submit reply will change transData, but we should not get this notifications
         transData->disableTransactionUpdate();
         submitReply(transData, qrn, price, cbSubmit, walletsManager_->getDefaultWallet());
         // remove circular reference in CB.
         transData->SetCallback({});
      }
   };

   const auto &cbFee = [qrn, transData, cbSubmit, txUpdated](float feePerByte) {
      transData->setFeePerByte(feePerByte);
      transData->SetCallback(txUpdated);
      // should force update
      transData->enableTransactionUpdate();
   };

   logger_->debug("[RFQDealerReply::onAQReply] start fee estimation for quote: {}"
      , qrn.quoteRequestId);
   walletsManager_->estimatedFeePerByte(2, cbFee, this);
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
   if (autoSignQuoteProvider_->autoSignState()) {
      ui_->comboBoxWallet->setCurrentText(autoSignQuoteProvider_->getAutoSignWalletName());
   }
   ui_->comboBoxWallet->setEnabled(!autoSignQuoteProvider_->autoSignState());
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
   const bool isXbt = (currentQRN_.assetType == bs::network::Asset::PrivateMarket && currentQRN_.side == bs::network::Side::Sell) ||
      ((currentQRN_.assetType == bs::network::Asset::SpotXBT) && (currentQRN_.side == bs::network::Side::Buy));

   QString totalBalance;
   if (isXbt) {
      if (!transactionData_) {
         totalBalance = noBalanceAvailable;
      }
      else {
         totalBalance = tr("%1 %2")
            .arg(UiUtils::displayAmount(transactionData_->GetTransactionSummary().availableBalance))
            .arg(QString::fromStdString(bs::network::XbtCurrency));
      }
   }
   else if ((currentQRN_.side == bs::network::Side::Buy) && (currentQRN_.assetType == bs::network::Asset::PrivateMarket)) {
      if (!ccCoinSel_) {
         totalBalance = noBalanceAvailable;
      }
      else {
         totalBalance = tr("%1 %2")
            .arg(UiUtils::displayCurrencyAmount(getPrivateMarketCoinBalance()))
            .arg(QString::fromStdString(baseProduct_));
      }
   }
   else {
      if (!assetManager_) {
         totalBalance = noBalanceAvailable;
      }
      else {
         totalBalance = tr("%1 %2")
            .arg(UiUtils::displayCurrencyAmount(assetManager_->getBalance(product_)))
            .arg(QString::fromStdString(currentQRN_.side == bs::network::Side::Buy ? baseProduct_ : product_));
      }
   }

   ui_->labelBalanceValue->setText(totalBalance);
}

BTCNumericTypes::balance_type bs::ui::RFQDealerReply::getPrivateMarketCoinBalance() const
{
   if (!ccCoinSel_) {
      return 0;
   }

   uint64_t balance = 0;
   for (const auto &utxo : dealerUtxoAdapter_->get(currentQRN_.quoteRequestId)) {
      balance += utxo.getValue();
   }
   if (!balance) {
      balance = ccCoinSel_->GetBalance();
   }

   return ccCoinSel_->GetWallet()->getTxBalance(balance);
}
