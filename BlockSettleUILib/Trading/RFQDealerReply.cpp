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

using namespace bs::ui;

constexpr int kSelectAQFileItemIndex = 1;

namespace {

   QString getDefaultScriptsDir()
   {
#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
      return QCoreApplication::applicationDirPath() + QStringLiteral("/scripts");
#else
      return QStringLiteral("/usr/share/blocksettle/scripts");
#endif
   }

} // namespace

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
   connect(ui_->checkBoxAQ, &ToggleSwitch::clicked, this, &RFQDealerReply::checkBoxAQClicked);
   connect(ui_->comboBoxAQScript, SIGNAL(activated(int)), this, SLOT(aqScriptChanged(int)));
   connect(ui_->pushButtonAdvanced, &QPushButton::clicked, this, &RFQDealerReply::showCoinControl);

   connect(ui_->comboBoxWallet, SIGNAL(currentIndexChanged(int)), this, SLOT(walletSelected(int)));
   connect(ui_->authenticationAddressComboBox, SIGNAL(currentIndexChanged(int)), SLOT(onAuthAddrChanged(int)));

   connect(ui_->checkBoxAutoSign, &ToggleSwitch::clicked, this, &RFQDealerReply::onAutoSignActivated);

   ui_->responseTitle->hide();
}

RFQDealerReply::~RFQDealerReply()
{
   bs::UtxoReservation::delAdapter(utxoAdapter_);
}

void RFQDealerReply::init(const std::shared_ptr<spdlog::logger> logger
   , const std::shared_ptr<AuthAddressManager> &authAddressManager
   , const std::shared_ptr<AssetManager>& assetManager
   , const std::shared_ptr<QuoteProvider>& quoteProvider
   , const std::shared_ptr<ApplicationSettings> &appSettings
   , const std::shared_ptr<ConnectionManager> &connectionManager
   , const std::shared_ptr<SignContainer> &container
   , const std::shared_ptr<ArmoryConnection> &armory
   , std::shared_ptr<MarketDataProvider> mdProvider)
{
   logger_ = logger;
   assetManager_ = assetManager;
   quoteProvider_ = quoteProvider;
   authAddressManager_ = authAddressManager;
   appSettings_ = appSettings;
   signingContainer_ = container;
   armory_ = armory;
   connectionManager_ = connectionManager;

   utxoAdapter_ = std::make_shared<bs::DealerUtxoResAdapter>(logger_, nullptr);
   connect(quoteProvider_.get(), &QuoteProvider::orderUpdated, utxoAdapter_.get(), &bs::OrderUtxoResAdapter::onOrder);
   connect(quoteProvider_.get(), &QuoteProvider::orderUpdated, this, &RFQDealerReply::onOrderUpdated);
   connect(utxoAdapter_.get(), &bs::OrderUtxoResAdapter::reservedUtxosChanged, this, &RFQDealerReply::onReservedUtxosChanged, Qt::QueuedConnection);

   aq_ = new UserScriptRunner(quoteProvider_, utxoAdapter_, signingContainer_,
      mdProvider, assetManager_, logger_, this);

   if (walletsManager_) {
      aq_->setWalletsManager(walletsManager_);
   }

   connect(aq_, &UserScriptRunner::aqScriptLoaded, this, &RFQDealerReply::onAqScriptLoaded);
   connect(aq_, &UserScriptRunner::failedToLoad, this, &RFQDealerReply::onAqScriptFailed);
   connect(aq_, &UserScriptRunner::sendQuote, this, &RFQDealerReply::onAQReply, Qt::QueuedConnection);
   connect(aq_, &UserScriptRunner::pullQuoteNotif, this, &RFQDealerReply::pullQuoteNotif, Qt::QueuedConnection);

   if (signingContainer_) {
      connect(signingContainer_.get(), &SignContainer::ready, this, &RFQDealerReply::onSignerStateUpdated, Qt::QueuedConnection);
      connect(signingContainer_.get(), &SignContainer::disconnected, this, &RFQDealerReply::onSignerStateUpdated, Qt::QueuedConnection);
      connect(signingContainer_.get(), &SignContainer::AutoSignStateChanged, this, &RFQDealerReply::onAutoSignStateChanged);
   }

   UtxoReservation::addAdapter(utxoAdapter_);

   auto botFileInfo = QFileInfo(getDefaultScriptsDir() + QStringLiteral("/RFQBot.qml"));
   if (botFileInfo.exists() && botFileInfo.isFile()) {
      auto list = appSettings_->get<QStringList>(ApplicationSettings::aqScripts);
      if (list.indexOf(botFileInfo.absoluteFilePath()) == -1) {
         list << botFileInfo.absoluteFilePath();
      }
      appSettings_->set(ApplicationSettings::aqScripts, list);
      const auto lastScript = appSettings_->get<QString>(ApplicationSettings::lastAqScript);
      if (lastScript.isEmpty()) {
         appSettings_->set(ApplicationSettings::lastAqScript, botFileInfo.absoluteFilePath());
      }

   }

   aqFillHistory();

   onSignerStateUpdated();
}

void RFQDealerReply::initUi()
{
   ui_->labelRecvAddr->hide();
   ui_->comboBoxRecvAddr->hide();
   ui_->authenticationAddressLabel->hide();
   ui_->authenticationAddressComboBox->hide();
   ui_->pushButtonSubmit->setEnabled(false);
   ui_->pushButtonPull->setEnabled(false);
   ui_->widgetWallet->hide();
   ui_->comboBoxAQScript->setFirstItemHidden(true);

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

   if (aq_) {
      aq_->setWalletsManager(walletsManager_);
   }

   auto updateAuthAddresses = [this] {
      UiUtils::fillAuthAddressesComboBox(ui_->authenticationAddressComboBox, authAddressManager_);
      onAuthAddrChanged(ui_->authenticationAddressComboBox->currentIndex());
   };
   updateAuthAddresses();
   connect(authAddressManager_.get(), &AuthAddressManager::VerifiedAddressListUpdated, this, updateAuthAddresses);
}

bool RFQDealerReply::autoSign() const
{
   return ui_->checkBoxAutoSign->isChecked();
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

void RFQDealerReply::onSignerStateUpdated()
{
   ui_->groupBoxAutoSign->setVisible(signingContainer_ && !signingContainer_->isOffline());
   disableAutoSign();
}

void RFQDealerReply::onAutoSignActivated()
{
   if (ui_->checkBoxAutoSign->isChecked()) {
      tryEnableAutoSign();
   } else {
      disableAutoSign();
   }
   ui_->checkBoxAutoSign->setChecked(autoSignState_);
}

bs::Address RFQDealerReply::getRecvAddress() const
{
   if (!curWallet_) {
      logger_->warn("[RFQDealerReply::getRecvAddress] no current wallet");
      return {};
   }

   const auto index = ui_->comboBoxRecvAddr->currentIndex();
   logger_->debug("[RFQDealerReply::getRecvAddress] obtaining addr #{} from wallet {}", index, curWallet_->name());
   if (index <= 0) {
      auto promAddr = std::make_shared<std::promise<bs::Address>>();
      auto futAddr = promAddr->get_future();
      const auto &cbAddr = [this, promAddr](const bs::Address &addr) {
         promAddr->set_value(addr);
         if (curWallet_->type() != bs::core::wallet::Type::ColorCoin) {
            curWallet_->setAddressComment(addr
               , bs::sync::wallet::Comment::toString(bs::sync::wallet::Comment::SettlementPayOut));
         }
      };
      curWallet_->getNewIntAddress(cbAddr);
//      curWallet_->RegisterWallet();  //TODO: invoke at address callback
      return futAddr.get();
   }
   return curWallet_->getExtAddressList()[index - 1];
}

void RFQDealerReply::updateRecvAddresses()
{
   if (prevWallet_ == curWallet_) {
      return;
   }

   ui_->comboBoxRecvAddr->clear();
   ui_->comboBoxRecvAddr->addItem(tr("Auto Create"));
   if (curWallet_ != nullptr) {
      for (const auto &addr : curWallet_->getExtAddressList()) {
         ui_->comboBoxRecvAddr->addItem(QString::fromStdString(addr.display()));
      }
   }
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
            transactionData_->setWallet(curWallet_, armory_->topBlock());
         }
         else if (currentQRN_.assetType == bs::network::Asset::PrivateMarket) {
            std::shared_ptr<bs::sync::Wallet> wallet;
            const auto xbtWallet = getXbtWallet();
            if (currentQRN_.side == bs::network::Side::Buy) {
               wallet = getCCWallet(currentQRN_.product);
            }
            else {
               wallet = xbtWallet;
            }
            if (wallet && (!ccCoinSel_ || (ccCoinSel_->GetWallet() != wallet))) {
               ccCoinSel_ = std::make_shared<SelectedTransactionInputs>(wallet, true, true);
            }
            transactionData_->setSigningWallet(wallet);
            transactionData_->setWallet(xbtWallet, armory_->topBlock());
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
   updateRecvAddresses();

   if (!qFuzzyIsNull(indicBid_)) {
      ui_->spinBoxBidPx->setValue(indicBid_);
   }
   else {
      ui_->spinBoxBidPx->clear();
   }

   if (!qFuzzyIsNull(indicAsk_)) {
      ui_->spinBoxOfferPx->setValue(indicAsk_);
   }
   else {
      ui_->spinBoxOfferPx->clear();
   }
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
   ui_->labelRecvAddr->setVisible(isXBT || isPrivMkt);
   ui_->comboBoxRecvAddr->setVisible(isXBT || isPrivMkt);

   dealerSellXBT_ = (isXBT || isPrivMkt) && ((qrn.product == bs::network::XbtCurrency) ^ (qrn.side == bs::network::Side::Sell));

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

std::shared_ptr<bs::sync::Wallet> RFQDealerReply::getCCWallet(const std::string &cc)
{
   if (ccWallet_ && (ccWallet_->shortName() == cc)) {
      return ccWallet_;
   }
   if (walletsManager_) {
      ccWallet_ = walletsManager_->getCCWallet(cc);
   }
   else {
      ccWallet_ = nullptr;
   }
   return ccWallet_;
}

std::shared_ptr<bs::sync::Wallet> RFQDealerReply::getXbtWallet()
{
   if (!xbtWallet_ && walletsManager_) {
      xbtWallet_ = walletsManager_->getDefaultWallet();
   }
   return xbtWallet_;
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
         } else if (ccWallet != curWallet_) {
            ui_->comboBoxWallet->clear();
            ui_->comboBoxWallet->addItem(QString::fromStdString(ccWallet->name()));
            ui_->comboBoxWallet->setItemData(0, QString::fromStdString(ccWallet->walletId()), UiUtils::WalletIdRole);
            setCurrentWallet(ccWallet);
         }
      }
      else {
         if (!curWallet_ || (ccWallet_ && (ccWallet_ == curWallet_))) {
            walletSelected(UiUtils::fillWalletsComboBox(ui_->comboBoxWallet, walletsManager_, signingContainer_));
         }
      }
   }
   else if (qrn.assetType == bs::network::Asset::SpotXBT) {
      walletSelected(UiUtils::fillWalletsComboBox(ui_->comboBoxWallet, walletsManager_, signingContainer_));
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
   if (authAddr_.isNull()) {
      return;
   }
   const auto priWallet = walletsManager_->getPrimaryWallet();
   const auto group = priWallet->getGroup(bs::hd::BlockSettle_Settlement);
   std::shared_ptr<bs::sync::hd::SettlementLeaf> settlLeaf;
   if (group) {
      const auto settlGroup = std::dynamic_pointer_cast<bs::sync::hd::SettlementGroup>(group);
      if (!settlGroup) {
         logger_->error("[{}] wrong settlement group type", __func__);
         return;
      }
      settlLeaf = settlGroup->getLeaf(authAddr_);
   }

   const auto &cbPubKey = [this](const SecureBinaryData &pubKey) {
      authKey_ = pubKey.toHexStr();
      QMetaObject::invokeMethod(this, &RFQDealerReply::updateSubmitButton);
   };

   if (settlLeaf) {
      settlLeaf->getRootPubkey(cbPubKey);
   } else {
      walletsManager_->createSettlementLeaf(authAddr_, cbPubKey);
      return;
   }
}

void RFQDealerReply::updateSubmitButton()
{
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

   if ((currentQRN_.side == bs::network::Side::Buy) ^ (product_ == baseProduct_)) {
      const auto amount = getAmount();
      if ((currentQRN_.assetType == bs::network::Asset::SpotXBT) && transactionData_) {
         return (amount <= (transactionData_->GetTransactionSummary().availableBalance
            - transactionData_->GetTransactionSummary().totalFee / BTCNumericTypes::BalanceDivider));
      }
      else if ((currentQRN_.assetType == bs::network::Asset::PrivateMarket) && ccCoinSel_) {
         uint64_t balance = 0;
         for (const auto &utxo : utxoAdapter_->get(currentQRN_.quoteRequestId)) {
            balance += utxo.getValue();
         }
         if (!balance) {
            balance = ccCoinSel_->GetBalance();
         }
         return (amount <= ccCoinSel_->GetWallet()->getTxBalance(balance));
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

void RFQDealerReply::setCurrentWallet(const std::shared_ptr<bs::sync::Wallet> &newWallet)
{
   prevWallet_ = curWallet_;
   curWallet_ = newWallet;

   if (newWallet != ccWallet_) {
      xbtWallet_ = newWallet;
   }

   if (newWallet != nullptr) {
      if (transactionData_ != nullptr) {
         transactionData_->setWallet(newWallet, armory_->topBlock());
      }
   }
}

void RFQDealerReply::walletSelected(int index)
{
   if (walletsManager_) {
      setCurrentWallet(walletsManager_->getWalletById(ui_->comboBoxWallet->currentData(UiUtils::WalletIdRole).toString().toStdString()));
   }

   updateRecvAddresses();
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

void RFQDealerReply::submitReply(const std::shared_ptr<TransactionData> transData
   , const bs::network::QuoteReqNotification &qrn, double price
   , std::function<void(bs::network::QuoteNotification)> cb)
{  //TODO: refactor to properly support asynchronicity of getChangeAddress
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

   const auto &lbdQuoteNotif = [this, cb, qrn, price, transData](const std::string &txData) {
      logger_->debug("[RFQDealerReply::submitReply] txData={}", txData);
      auto qn = std::make_shared<bs::network::QuoteNotification>(qrn, authKey_, price, txData);

      if (qrn.assetType == bs::network::Asset::PrivateMarket) {
         qn->receiptAddress = getRecvAddress().display();
         qn->reqAuthKey = qrn.requestorRecvAddress;

         auto wallet = transData->getSigningWallet();
         auto spendVal = std::make_shared<uint64_t>();
         *spendVal = 0;

         const auto &cbFee = [this, qrn, transData, spendVal, wallet, cb, qn](float feePerByte) {
            const auto recipient = bs::Address(qrn.requestorRecvAddress).getRecipient(*spendVal);
            std::vector<UTXO> inputs = utxoAdapter_->get(qn->quoteRequestId);
            if (inputs.empty() && ccCoinSel_) {
               inputs = ccCoinSel_->GetSelectedTransactions();
               if (inputs.empty()) {
                  logger_->error("[RFQDealerReply::submit] no suitable inputs for CC sell");
                  cb({});
                  return;
               }
            }
            try {
               auto promAddr = std::make_shared<std::promise<bs::Address>>();
               auto futAddr = promAddr->get_future();
               const auto &cbAddr = [promAddr](const bs::Address &addr) {
                  promAddr->set_value(addr);
               };
               wallet->getNewChangeAddress(cbAddr);
               logger_->debug("[cbFee] {} input[s], fpb={}, recip={}, prevPart={}", inputs.size(), feePerByte
                  , bs::Address(qrn.requestorRecvAddress).display(), qrn.requestorAuthPublicKey);
               try {
                  Signer signer;
                  signer.deserializeState(BinaryData::CreateFromHex(qrn.requestorAuthPublicKey));
                  logger_->debug("[cbFee] deserialized state");
               } catch (const std::exception &e) {
                  logger_->error("[cbFee] state deser failed: {}", e.what());
               }
               const auto txReq = wallet->createPartialTXRequest(*spendVal, inputs, futAddr.get(), feePerByte
                  , { recipient }, BinaryData::CreateFromHex(qrn.requestorAuthPublicKey));
               qn->transactionData = txReq.serializeState().toHexStr();
               logger_->debug("[cbFee] txData={}", qn->transactionData);
               utxoAdapter_->reserve(txReq, qn->quoteRequestId);
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

         if (qrn.side == bs::network::Side::Buy) {
            if (!wallet) {
               wallet = getCCWallet(qrn.product);
            }
            *spendVal = qrn.quantity * assetManager_->getCCLotSize(qrn.product);
            cbFee(0);
            return;
         } else {
            if (!wallet) {
               wallet = getXbtWallet();
            }
            *spendVal = qrn.quantity * price * BTCNumericTypes::BalanceDivider;
            walletsManager_->estimatedFeePerByte(2, cbFee, this);
            return;
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
                  bs::core::wallet::TXSignRequest unsignedTxReq;
                  if (transData->GetTransactionSummary().hasChange) {
                     auto promAddr = std::make_shared<std::promise<bs::Address>>();
                     auto futAddr = promAddr->get_future();
                     const auto &cbAddr = [promAddr, transData](const bs::Address &addr) {
                        promAddr->set_value(addr);
                        transData->getWallet()->setAddressComment(addr, bs::sync::wallet::Comment::toString(
                           bs::sync::wallet::Comment::ChangeAddress));
                     };
                     transData->getWallet()->getNewChangeAddress(cbAddr);
                     unsignedTxReq = transData->createUnsignedTransaction(false, futAddr.get());
                  } else {
                     unsignedTxReq = transData->createUnsignedTransaction();
                  }
                  quoteProvider_->saveDealerPayin(qrn.settlementId, unsignedTxReq.serializeState());
                  utxoAdapter_->reserve(unsignedTxReq, qrn.settlementId);

                  const auto txData = unsignedTxReq.txId().toHexStr();
                  lbdQuoteNotif(txData);
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
      }
      else {
         settlLeaf->setSettlementID(settlementId, [](bool) {});
         transData->SetFallbackRecvAddress(getRecvAddress());
      }
   }
   lbdQuoteNotif({});
}

void RFQDealerReply::tryEnableAutoSign()
{
   ui_->checkBoxAQ->setChecked(false);

   if (!walletsManager_ || !signingContainer_) {
      return;
   }

   const auto wallet = walletsManager_->getPrimaryWallet();
   if (!wallet) {
      logger_->error("Failed to obtain auto-sign primary wallet");
      return;
   }

   QVariantMap data;
   data[QLatin1String("rootId")] = QString::fromStdString(wallet->walletId());
   data[QLatin1String("enable")] = true;
   signingContainer_->customDialogRequest(bs::signer::ui::DialogType::ActivateAutoSign, data);
}

void RFQDealerReply::disableAutoSign()
{
   if (!walletsManager_) {
      return;
   }

   const auto wallet = walletsManager_->getPrimaryWallet();
   if (!wallet) {
      logger_->error("Failed to obtain auto-sign primary wallet");
      return;
   }

   QVariantMap data;
   data[QLatin1String("rootId")] = QString::fromStdString(wallet->walletId());
   data[QLatin1String("enable")] = false;
   signingContainer_->customDialogRequest(bs::signer::ui::DialogType::ActivateAutoSign, data);
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
         QMetaObject::invokeMethod(this, [this, qn] { emit submitQuoteNotif(qn); });
      }
   };
   submitReply(transactionData_, currentQRN_, price, cbSubmit);
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

QString RFQDealerReply::askForAQScript()
{
   auto lastDir = appSettings_->get<QString>(ApplicationSettings::LastAqDir);
   if (lastDir.isEmpty()) {
      lastDir = getDefaultScriptsDir();
   }

   auto path = QFileDialog::getOpenFileName(this, tr("Open Auto-quoting script file")
      , lastDir, tr("QML files (*.qml)"));

   if (!path.isEmpty()) {
      appSettings_->set(ApplicationSettings::LastAqDir, QFileInfo(path).dir().absolutePath());
   }

   return path;
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

   if (aq_) {
      return aq_->getTransactionData(reqId);
   } else {
      return nullptr;
   }
}

void RFQDealerReply::validateGUI()
{
   updateSubmitButton();

   ui_->checkBoxAQ->setChecked(aqLoaded_);

   // enable toggleswitch only if a script file is already selected
   bool isValidScript = (ui_->comboBoxAQScript->currentIndex() > kSelectAQFileItemIndex);
   if (!(isValidScript && celerConnected_)) {
      ui_->checkBoxAQ->setChecked(false);
   }
   ui_->comboBoxAQScript->setEnabled(celerConnected_);
   ui_->groupBoxAutoSign->setEnabled(celerConnected_);


   bool bFlag = walletsManager_ && walletsManager_->getPrimaryWallet();
   ui_->checkBoxAutoSign->setEnabled(bFlag && celerConnected_);
}

void RFQDealerReply::onTransactionDataChanged()
{
   QMetaObject::invokeMethod(this, &RFQDealerReply::updateSubmitButton);
}

void RFQDealerReply::initAQ(const QString &filename)
{
   if (filename.isEmpty()) {
      return;
   }
   aqLoaded_ = false;
   aq_->enableAQ(filename);
   validateGUI();
}

void RFQDealerReply::deinitAQ()
{
   aq_->disableAQ();
   aqLoaded_ = false;
   validateGUI();
}

void RFQDealerReply::aqFillHistory()
{
   if (!appSettings_) {
      return;
   }
   ui_->comboBoxAQScript->clear();
   int curIndex = 0;
   ui_->comboBoxAQScript->addItem(tr("Select script..."));
   ui_->comboBoxAQScript->addItem(tr("Load new AQ script"));
   const auto scripts = appSettings_->get<QStringList>(ApplicationSettings::aqScripts);
   if (!scripts.isEmpty()) {
      const auto lastScript = appSettings_->get<QString>(ApplicationSettings::lastAqScript);
      for (int i = 0; i < scripts.size(); i++) {
         QFileInfo fi(scripts[i]);
         ui_->comboBoxAQScript->addItem(fi.fileName(), scripts[i]);
         if (scripts[i] == lastScript) {
            curIndex = i + kSelectAQFileItemIndex + 1; // note the "Load" row in the head
         }
      }
   }
   ui_->comboBoxAQScript->setCurrentIndex(curIndex);
}

void RFQDealerReply::aqScriptChanged(int curIndex)
{
   if (curIndex < kSelectAQFileItemIndex) {
      return;
   }

   if (curIndex == kSelectAQFileItemIndex) {
      const auto scriptFN = askForAQScript();

      if (scriptFN.isEmpty()) {
         aqFillHistory();
         return;
      }

      // comboBoxAQScript will be updated later from onAqScriptLoaded
      newLoaded_ = true;
      initAQ(scriptFN);
   } else {
      if (aqLoaded_) {
         deinitAQ();
      }
   }
}

void RFQDealerReply::onAqScriptLoaded(const QString &filename)
{
   logger_->info("AQ script loaded ({})", filename.toStdString());

   auto scripts = appSettings_->get<QStringList>(ApplicationSettings::aqScripts);
   if (scripts.indexOf(filename) < 0) {
      scripts << filename;
      appSettings_->set(ApplicationSettings::aqScripts, scripts);
   }
   appSettings_->set(ApplicationSettings::lastAqScript, filename);
   aqFillHistory();

   if (newLoaded_) {
      newLoaded_ = false;
      deinitAQ();
   } else {
      aqLoaded_ = true;
      validateGUI();
   }
}

void RFQDealerReply::onAqScriptFailed(const QString &filename, const QString &error)
{
   logger_->error("AQ script loading failed (): {}", filename.toStdString(), error.toStdString());
   aqLoaded_ = false;

   auto scripts = appSettings_->get<QStringList>(ApplicationSettings::aqScripts);
   scripts.removeOne(filename);
   appSettings_->set(ApplicationSettings::aqScripts, scripts);
   appSettings_->reset(ApplicationSettings::lastAqScript);
   aqFillHistory();

   validateGUI();
}

void RFQDealerReply::checkBoxAQClicked()
{
   bool isValidScript = (ui_->comboBoxAQScript->currentIndex() > kSelectAQFileItemIndex);
   if (ui_->checkBoxAQ->isChecked() && !isValidScript) {
      BSMessageBox question(BSMessageBox::question
         , tr("Try to enable Auto Quoting")
         , tr("Auto Quoting Script is not specified. Do you want to select a script from file?"));
      const bool answerYes = (question.exec() == QDialog::Accepted);
      if (answerYes) {
         const auto scriptFileName = askForAQScript();
         if (scriptFileName.isEmpty()) {
            ui_->checkBoxAQ->setChecked(false);
         } else {
            initAQ(scriptFileName);
         }
      } else {
         ui_->checkBoxAQ->setChecked(false);
      }
   }

   if (aqLoaded_) {
      aq_->disableAQ();
      aqLoaded_ = false;
   } else {
      initAQ(ui_->comboBoxAQScript->currentData().toString());
   }

   validateGUI();
}

void RFQDealerReply::onOrderUpdated(const bs::network::Order &order)
{
   if ((order.assetType == bs::network::Asset::PrivateMarket) && (order.status == bs::network::Order::Failed)) {
      const auto &quoteReqId = quoteProvider_->getQuoteReqId(order.quoteId);
      if (!quoteReqId.empty()) {
         utxoAdapter_->unreserve(quoteReqId);
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

   if (qrn.assetType != bs::network::Asset::SpotFX) {
      auto wallet = getCurrentWallet();
      if (!wallet) {
         wallet = walletsManager_->getDefaultWallet();
      }

      transData = std::make_shared<TransactionData>(TransactionData::onTransactionChanged{}, logger_, true, true);

      transData->disableTransactionUpdate();
      transData->setWallet(wallet, armory_->topBlock());

      if (qrn.assetType == bs::network::Asset::PrivateMarket) {
         const auto &cc = qrn.product;
         const auto& ccWallet = getCCWallet(cc);
         if (qrn.side == bs::network::Side::Buy) {
            transData->setSigningWallet(ccWallet);
            curWallet_ = wallet;
         } else {
            if (!ccWallet) {
               ui_->checkBoxAQ->setChecked(false);
               BSMessageBox(BSMessageBox::critical, tr("Auto Quoting")
                  , tr("No wallet created for %1 - auto-quoting disabled").arg(QString::fromStdString(cc))
               ).exec();
               return;
            }
            transData->setSigningWallet(wallet);
            curWallet_ = ccWallet;
         }
      }

      const auto txUpdated = [this, qrn, price, cbSubmit, transData]()
      {
         logger_->debug("[RFQDealerReply::onAQReply TX CB] : tx updated for {} - {}"
            , qrn.quoteRequestId, (transData->InputsLoadedFromArmory() ? "inputs loaded" : "inputs not loaded"));

         if (transData->InputsLoadedFromArmory()) {
            aq_->setTxData(qrn.quoteRequestId, transData);
            // submit reply will change transData, but we should not get this notifications
            transData->disableTransactionUpdate();
            submitReply(transData, qrn, price, cbSubmit);
            // remove circular reference in CB.
            transData->SetCallback({});
         }
      };

      const auto &cbFee = [this, qrn, price, transData, cbSubmit, txUpdated](float feePerByte) {
         transData->setFeePerByte(feePerByte);
         transData->SetCallback(txUpdated);
         // should force update
         transData->enableTransactionUpdate();
      };

      logger_->debug("[RFQDealerReply::onAQReply] start fee estimation for quote: {}"
         , qrn.quoteRequestId);
      walletsManager_->estimatedFeePerByte(2, cbFee, this);
      return;
   }

   submitReply(transData, qrn, price, cbSubmit);
}

void RFQDealerReply::onAutoSignStateChanged(const std::string &walletId, bool active)
{
   autoSignState_ = active;
   ui_->checkBoxAutoSign->setChecked(active);
   validateGUI();
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

   ccWallet_ = ccLeaf;
   updateUiWalletFor(currentQRN_);
   reset();
   updateRecvAddresses();
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
   aq_->disableAQ();
   disableAutoSign();
   validateGUI();
}
