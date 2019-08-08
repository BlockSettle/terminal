#include "RFQTicketXBT.h"
#include "ui_RFQTicketXBT.h"

#include <QComboBox>
#include <QEvent>
#include <QKeyEvent>
#include <QLineEdit>

#include "AssetManager.h"
#include "AuthAddressManager.h"
#include "BSErrorCodeStrings.h"
#include "BSMessageBox.h"
#include "CCAmountValidator.h"
#include "CoinControlDialog.h"
#include "CoinSelection.h"
#include "CurrencyPair.h"
#include "EncryptionUtils.h"
#include "FXAmountValidator.h"
#include "QuoteProvider.h"
#include "SelectedTransactionInputs.h"
#include "SignContainer.h"
#include "TxClasses.h"
#include "UiUtils.h"
#include "UtxoReserveAdapters.h"
#include "Wallets/SyncHDWallet.h"
#include "Wallets/SyncWalletsManager.h"
#include "XbtAmountValidator.h"

#include <cstdlib>

static const QString EmptyInformationalLabelText = QString::fromStdString("--");

RFQTicketXBT::RFQTicketXBT(QWidget* parent)
   : QWidget(parent)
   , ui_(new Ui::RFQTicketXBT())
{
   ui_->setupUi(this);

   initProductGroupMap();

   invalidBalanceFont_ = ui_->labelBalanceValue->font();
   invalidBalanceFont_.setStrikeOut(true);

   ui_->pushButtonCreateWallet->hide();
   ui_->pushButtonCreateWallet->setEnabled(false);

   ccAmountValidator_ = new CCAmountValidator(this);
   fxAmountValidator_ = new FXAmountValidator(this);
   xbtAmountValidator_ = new XbtAmountValidator(this);

   ui_->lineEditAmount->installEventFilter(this);

   connect(ui_->pushButtonNumCcy, &QPushButton::clicked, this, &RFQTicketXBT::onNumCcySelected);
   connect(ui_->pushButtonDenomCcy, &QPushButton::clicked, this, &RFQTicketXBT::onDenomCcySelected);

   connect(ui_->pushButtonSell, &QPushButton::clicked, this, &RFQTicketXBT::onSellSelected);
   connect(ui_->pushButtonBuy, &QPushButton::clicked, this, &RFQTicketXBT::onBuySelected);

   connect(ui_->pushButtonSubmit, &QPushButton::clicked, this, &RFQTicketXBT::submitButtonClicked);
   connect(ui_->toolButtonXBTInputs, &QPushButton::clicked, this, &RFQTicketXBT::showCoinControl);
   connect(ui_->toolButtonMax, &QPushButton::clicked, this, &RFQTicketXBT::onMaxClicked);
   connect(ui_->comboBoxXBTWallets, SIGNAL(currentIndexChanged(int)), this, SLOT(walletSelected(int)));

   connect(ui_->pushButtonCreateWallet, &QPushButton::clicked, this, &RFQTicketXBT::onCreateWalletClicked);

   connect(ui_->lineEditAmount, &QLineEdit::textEdited, this, &RFQTicketXBT::onAmountEdited);

   connect(ui_->authenticationAddressComboBox, SIGNAL(currentIndexChanged(int)), SLOT(onAuthAddrChanged(int)));
   connect(this, &RFQTicketXBT::update, this, &RFQTicketXBT::onTransactinDataChanged);

   ui_->comboBoxCCWallets->setEnabled(false);
   ui_->comboBoxXBTWallets->setEnabled(false);

   disablePanel();
}

RFQTicketXBT::~RFQTicketXBT()
{
   bs::UtxoReservation::delAdapter(utxoAdapter_);
}

void RFQTicketXBT::setTransactionData()
{
   if (!transactionData_) {
      transactionData_ = std::make_shared<TransactionData>([this]() { emit update(); }
         , nullptr, true, true);
   }

   if (walletsManager_ != nullptr) {
      const auto &cbFee = [this](float feePerByte) {
         if (!transactionData_) {
            return;
         }
         transactionData_->setFeePerByte(feePerByte);
         setWallets();
      };
      walletsManager_->estimatedFeePerByte(2, cbFee, this);
   }
}

void RFQTicketXBT::setWallets()
{
   const auto prevRecvWallet = recvWallet_;
   if (currentGroupType_ == ProductGroupType::CCGroupType) {
      const auto &product = getProduct();
      if (!product.isEmpty()) {
         std::shared_ptr<bs::sync::Wallet> wallet;
         const auto side = getSelectedSide();
         if (side == bs::network::Side::Sell) {
            wallet = getCCWallet(product.toStdString());
            recvWallet_ = curWallet_;
         } else if (side == bs::network::Side::Buy) {
            wallet = curWallet_;
            recvWallet_ = getCCWallet(product.toStdString());
         }
         transactionData_->setSigningWallet(wallet);
      }
   } else {
      recvWallet_ = curWallet_;
   }

   transactionData_->setWallet(curWallet_, armory_->topBlock());
   if (prevRecvWallet != recvWallet_) {
      fillRecvAddresses();
   }
}

void RFQTicketXBT::resetTicket()
{
   ui_->labelProductGroup->setText(EmptyInformationalLabelText);
   ui_->labelSecurityId->setText(EmptyInformationalLabelText);
   ui_->labelIndicativePrice->setText(EmptyInformationalLabelText);

   currentBidPrice_ = EmptyInformationalLabelText;
   currentOfferPrice_ = EmptyInformationalLabelText;
   currentGroupType_ = ProductGroupType::GroupNotSelected;

   ui_->lineEditAmount->setValidator(nullptr);
   ui_->lineEditAmount->setEnabled(false);
   ui_->lineEditAmount->clear();

   transactionData_ = nullptr;
   rfqMap_.clear();

   HideRFQControls();

   updatePanel();
}

void RFQTicketXBT::init(const std::shared_ptr<AuthAddressManager> &authAddressManager
   , const std::shared_ptr<AssetManager>& assetManager, const std::shared_ptr<QuoteProvider> &quoteProvider
   , const std::shared_ptr<SignContainer> &container, const std::shared_ptr<ArmoryConnection> &armory)
{
   authAddressManager_ = authAddressManager;
   assetManager_ = assetManager;
   signingContainer_ = container;
   armory_ = armory;

   utxoAdapter_ = std::make_shared<bs::RequesterUtxoResAdapter>(nullptr, this);
   connect(quoteProvider.get(), &QuoteProvider::orderUpdated, utxoAdapter_.get(), &bs::OrderUtxoResAdapter::onOrder);
   connect(utxoAdapter_.get(), &bs::OrderUtxoResAdapter::reservedUtxosChanged, this, &RFQTicketXBT::onReservedUtxosChanged, Qt::QueuedConnection);
   bs::UtxoReservation::addAdapter(utxoAdapter_);

   if (signingContainer_) {
      connect(signingContainer_.get(), &SignContainer::ready, this, &RFQTicketXBT::onSignerReady);
   }

   updateSubmitButton();
}

void RFQTicketXBT::onReservedUtxosChanged(const std::string &walletId, const std::vector<UTXO> &utxos)
{
   if (ccCoinSel_ && (ccCoinSel_->GetWallet()->walletId() == walletId)) {
      ccCoinSel_->Reload(utxos);
   } else if (transactionData_
     && transactionData_->getWallet()
     && (transactionData_->getWallet()->walletId() == walletId)) {
      transactionData_->ReloadSelection(utxos);
   }
   updateBalances();
   updateSubmitButton();
}

std::shared_ptr<bs::sync::Wallet> RFQTicketXBT::getCCWallet(const std::string &cc)
{
   if (ccWallet_ && (ccWallet_->shortName() == cc)) {
      return ccWallet_;
   }

   if (walletsManager_) {
      return walletsManager_->getCCWallet(cc);
   }

   return nullptr;
}

void RFQTicketXBT::updatePanel()
{
   const auto selectedSide = getSelectedSide();

   if (selectedSide == bs::network::Side::Undefined) {
      showHelp(tr("Click on desired product in MD list"));
      ui_->pushButtonSubmit->setEnabled(false);
      return;
   }

   ui_->toolButtonMax->setVisible(selectedSide == bs::network::Side::Sell);

   if (currentGroupType_ != ProductGroupType::FXGroupType) {
      bool buyXBT = isXBTProduct() ^ (selectedSide == bs::network::Side::Sell);
      ui_->toolButtonXBTInputs->setVisible(!buyXBT);
      ui_->recAddressLayout->setVisible(buyXBT);
   }

   updateIndicativePrice();
   updateBalances();
   updateSubmitButton();
}

void RFQTicketXBT::setCurrentCCWallet(const std::shared_ptr<bs::sync::Wallet>& newCCWallet)
{
   if (newCCWallet != nullptr) {
      ccWallet_ = newCCWallet;
      ccCoinSel_ = std::make_shared<SelectedTransactionInputs>(ccWallet_, true, true);
   } else {
      ccWallet_ = nullptr;
      ccCoinSel_ = nullptr;
   }
}

void RFQTicketXBT::onHDLeafCreated(const std::string& ccName)
{
   if (getProduct().toStdString() != ccName) {
      return;
   }

   auto leaf = walletsManager_->getCCWallet(ccName);
   if (leaf == nullptr) {
      showHelp(tr("Leaf not created"));
      return;
   }

   ui_->comboBoxCCWallets->clear();
   ui_->comboBoxCCWallets->addItem(QString::fromStdString(leaf->name()));
   ui_->comboBoxCCWallets->setItemData(0, QString::fromStdString(leaf->walletId()), UiUtils::WalletIdRole);
   ui_->comboBoxCCWallets->setCurrentIndex(0);

   ui_->pushButtonCreateWallet->hide();
   ui_->pushButtonCreateWallet->setText(tr("Create Wallet"));
   ui_->pushButtonSubmit->show();

   ui_->comboBoxCCWallets->setEnabled(true);
   setCurrentCCWallet(leaf);
   clearHelp();
   updatePanel();
}

void RFQTicketXBT::onCreateHDWalletError(const std::string& ccName, bs::error::ErrorCode result)
{
   if (getProduct().toStdString() != ccName) {
      return;
   }

   showHelp(tr("Failed to create wallet: %1").arg(bs::error::ErrorCodeToString(result)));
}

void RFQTicketXBT::updateBalances()
{
   const auto balance = getBalanceInfo();

   QString amountString;
   switch (balance.productType) {
   case ProductGroupType::XBTGroupType:
      amountString = UiUtils::displayAmount(balance.amount);
      break;
   case ProductGroupType::CCGroupType:
      amountString = UiUtils::displayCCAmount(balance.amount);
      break;
   case ProductGroupType::FXGroupType:
      amountString = UiUtils::displayCurrencyAmount(balance.amount);
      break;
   }

   QString text = tr("%1 %2").arg(amountString).arg(balance.product);
   ui_->labelBalanceValue->setText(text);
}

RFQTicketXBT::BalanceInfoContainer RFQTicketXBT::getBalanceInfo() const
{
   BalanceInfoContainer balance;

   QString productToSpend;

   if (getSelectedSide() == bs::network::Side::Sell) {
      productToSpend = currentProduct_;
   } else {
      productToSpend = contraProduct_;
   }

   if (UiUtils::XbtCurrency == productToSpend) {
      balance.amount = transactionData_ ? transactionData_->GetTransactionSummary().availableBalance : 0;
      balance.product = UiUtils::XbtCurrency;
      balance.productType = ProductGroupType::XBTGroupType;
   } else {
      if (currentGroupType_ == ProductGroupType::CCGroupType) {
         balance.amount = ccCoinSel_ ? ccCoinSel_->GetWallet()->getTxBalance(ccCoinSel_->GetBalance()) : 0;
         balance.product = productToSpend;
         balance.productType = ProductGroupType::CCGroupType;
      } else {
         const double divisor = std::pow(10, UiUtils::GetAmountPrecisionFX());
         const double intBalance = std::floor((assetManager_ ? assetManager_->getBalance(productToSpend.toStdString()) : 0.0) * divisor);
         balance.amount = intBalance / divisor;
         balance.product = productToSpend;
         balance.productType = ProductGroupType::FXGroupType;
      }
   }

   return balance;
}

QString RFQTicketXBT::getProduct() const
{
   return currentProduct_;
}

bool RFQTicketXBT::isXBTProduct() const
{
   return (getProduct() == UiUtils::XbtCurrency);
}

void RFQTicketXBT::setWalletsManager(const std::shared_ptr<bs::sync::WalletsManager> &walletsManager)
{
   walletsManager_ = walletsManager;
   connect(walletsManager_.get(), &bs::sync::WalletsManager::walletsSynchronized, this, &RFQTicketXBT::walletsLoaded);

   connect(walletsManager_.get(), &bs::sync::WalletsManager::CCLeafCreated, this, &RFQTicketXBT::onHDLeafCreated);
   connect(walletsManager_.get(), &bs::sync::WalletsManager::CCLeafCreateFailed, this, &RFQTicketXBT::onCreateHDWalletError);

   walletsLoaded();

   auto updateAuthAddresses = [this] {
      UiUtils::fillAuthAddressesComboBox(ui_->authenticationAddressComboBox, authAddressManager_);
      onAuthAddrChanged(ui_->authenticationAddressComboBox->currentIndex());
   };
   updateAuthAddresses();
   connect(authAddressManager_.get(), &AuthAddressManager::VerifiedAddressListUpdated, this, updateAuthAddresses);
}

void RFQTicketXBT::walletsLoaded()
{
   if (!signingContainer_ || !walletsManager_ || !walletsManager_->hdWalletsCount()) {
      return;
   }
   ui_->comboBoxXBTWallets->clear();
   if (signingContainer_->isOffline()) {
      ui_->comboBoxXBTWallets->setEnabled(false);
   }
   else {
      ui_->comboBoxXBTWallets->setEnabled(true);
      walletSelected(UiUtils::fillWalletsComboBox(ui_->comboBoxXBTWallets, walletsManager_, signingContainer_));
   }
}

void RFQTicketXBT::onSignerReady()
{
   updateSubmitButton();
   ui_->receivingWalletWidget->setEnabled(!signingContainer_->isOffline());
}

void RFQTicketXBT::fillRecvAddresses()
{
   ui_->receivingAddressComboBox->clear();
   if (recvWallet_) {
      ui_->receivingAddressComboBox->addItem(tr("Auto Create"));
      for (auto addr : recvWallet_->getExtAddressList()) {
         ui_->receivingAddressComboBox->addItem(QString::fromStdString(addr.display()));
      }
      ui_->receivingAddressComboBox->setEnabled(true);
      ui_->receivingAddressComboBox->setCurrentIndex(0);
   } else {
      ui_->receivingAddressComboBox->setEnabled(false);
   }
}

void RFQTicketXBT::showCoinControl()
{
   if (transactionData_ && transactionData_->GetSelectedInputs()) {
      CoinControlDialog(transactionData_->GetSelectedInputs(), true, this).exec();
   }
}

void RFQTicketXBT::setCurrentWallet(const std::shared_ptr<bs::sync::Wallet> &newWallet)
{
   curWallet_ = newWallet;

   setTransactionData();
}

void RFQTicketXBT::walletSelected(int index)
{
   if (index == -1) {
      return;
   }

   if (walletsManager_) {
      setCurrentWallet(walletsManager_->getWalletById(ui_->comboBoxXBTWallets->currentData(UiUtils::WalletIdRole).toString().toStdString()));
   }
}

void RFQTicketXBT::SetProductGroup(const QString& productGroup)
{
   currentGroupType_ = getProductGroupType(productGroup);
   if (currentGroupType_ != ProductGroupType::GroupNotSelected) {
      ui_->labelProductGroup->setText(productGroup);

      ui_->lineBeforeProduct->setVisible(true);
      ui_->verticalWidgetSelectedProduct->setVisible(true);

      ui_->lineBeforeBalance->setVisible(true);
      ui_->balanceLayout->setVisible(true);

      if (currentGroupType_ != ProductGroupType::FXGroupType) {
         ui_->groupBoxSettlementInputs->setVisible(true);

         ui_->CCWalletLayout->setVisible(currentGroupType_ == ProductGroupType::CCGroupType);
         ui_->authAddressLayout->setVisible(currentGroupType_ == ProductGroupType::XBTGroupType);
      } else {
         ui_->groupBoxSettlementInputs->setVisible(false);
      }
   } else {
      ui_->labelProductGroup->setText(tr("XXX"));
   }
}

void RFQTicketXBT::SetCurrencyPair(const QString& currencyPair)
{
   if (currentGroupType_ != ProductGroupType::GroupNotSelected) {
      clearHelp();

      ui_->labelSecurityId->setText(currencyPair);

      CurrencyPair cp(currencyPair.toStdString());

      currentProduct_ = QString::fromStdString(cp.NumCurrency());
      contraProduct_ = QString::fromStdString(cp.DenomCurrency());

      ui_->pushButtonNumCcy->setText(currentProduct_);
      ui_->pushButtonNumCcy->setChecked(true);

      ui_->pushButtonDenomCcy->setText(contraProduct_);
      ui_->pushButtonDenomCcy->setChecked(false);

      ui_->pushButtonDenomCcy->setEnabled(currentGroupType_ != ProductGroupType::CCGroupType);
   }
}

void RFQTicketXBT::SetProductAndSide(const QString& productGroup, const QString& currencyPair
   , const QString& bidPrice, const QString& offerPrice, bs::network::Side::Type side)
{
   resetTicket();

   if (productGroup.isEmpty() || currencyPair.isEmpty()) {
      return;
   }

   SetProductGroup(productGroup);
   SetCurrencyPair(currencyPair);
   SetCurrentIndicativePrices(bidPrice, offerPrice);

  if (side == bs::network::Side::Type::Undefined) {
     side = getLastSideSelection(getProduct().toStdString(), currencyPair.toStdString());
  }

  ui_->pushButtonSell->setChecked(side == bs::network::Side::Sell);
  ui_->pushButtonBuy->setChecked(side == bs::network::Side::Buy);

  productSelectionChanged();
}

void RFQTicketXBT::setSecurityId(const QString& productGroup, const QString& currencyPair
   , const QString& bidPrice, const QString& offerPrice)
{
   SetProductAndSide(productGroup, currencyPair, bidPrice, offerPrice, bs::network::Side::Undefined);
}

void RFQTicketXBT::setSecurityBuy(const QString& productGroup, const QString& currencyPair
   , const QString& bidPrice, const QString& offerPrice)
{
   SetProductAndSide(productGroup, currencyPair, bidPrice, offerPrice, bs::network::Side::Buy);
}

void RFQTicketXBT::setSecuritySell(const QString& productGroup, const QString& currencyPair
   , const QString& bidPrice, const QString& offerPrice)
{
   SetProductAndSide(productGroup, currencyPair, bidPrice, offerPrice, bs::network::Side::Sell);
}

bs::network::Side::Type RFQTicketXBT::getSelectedSide() const
{
   if (currentGroupType_ == ProductGroupType::GroupNotSelected) {
      return bs::network::Side::Undefined;
   }

   if (ui_->pushButtonSell->isChecked()) {
      return bs::network::Side::Sell;
   }

   return bs::network::Side::Buy;
}

void RFQTicketXBT::onAuthAddrChanged(int index)
{
   const auto authAddr = authAddressManager_->GetAddress(authAddressManager_->FromVerifiedIndex(index));
   if (authAddr.isNull()) {
      return;
   }
   const auto priWallet = walletsManager_->getPrimaryWallet();
   const auto group = priWallet->getGroup(bs::hd::BlockSettle_Settlement);
   std::shared_ptr<bs::sync::hd::SettlementLeaf> settlLeaf;
   if (group) {
      const auto settlGroup = std::dynamic_pointer_cast<bs::sync::hd::SettlementGroup>(group);
      if (!settlGroup) {
         SPDLOG_ERROR("wrong settlement group type");
         return;
      }
      settlLeaf = settlGroup->getLeaf(authAddr);
   }

   const auto &cbPubKey = [this](const SecureBinaryData &pubKey) {
      authKey_ = pubKey.toHexStr();
      QMetaObject::invokeMethod(this, &RFQTicketXBT::updateSubmitButton);
   };

   if (settlLeaf) {
      settlLeaf->getRootPubkey(cbPubKey);
   }
   else {
      walletsManager_->createSettlementLeaf(authAddr, cbPubKey);
      return;
   }
}

bs::Address RFQTicketXBT::recvAddress() const
{
   const auto index = ui_->receivingAddressComboBox->currentIndex();
   if ((index < 0) || !recvWallet_) {
      return BinaryData();
   }

   if (index == 0) {
      auto promAddr = std::make_shared<std::promise<bs::Address>>();
      auto futAddr = promAddr->get_future();
      const auto &cbAddr = [promAddr](const bs::Address &addr) {
         promAddr->set_value(addr);
      };
      recvWallet_->getNewExtAddress(cbAddr);
      return futAddr.get();
   }
   return recvWallet_->getExtAddressList()[index - 1];
}

void RFQTicketXBT::onTransactinDataChanged()
{
   updateBalances();
   updateSubmitButton();
}

bool RFQTicketXBT::checkBalance(double qty) const
{
   if (getSelectedSide() == bs::network::Side::Buy) {
      return true;
   }
   const auto balance = getBalanceInfo();
   switch (balance.productType) {
   case ProductGroupType::XBTGroupType:
      return ((qty + estimatedFee()) <= balance.amount);
   case ProductGroupType::CCGroupType:
   case ProductGroupType::FXGroupType:
      return (qty <= balance.amount);
   default: break;
   }
   return false;
}

void RFQTicketXBT::updateSubmitButton()
{
   ui_->pushButtonSubmit->setEnabled(false);

   if (!assetManager_) {
      return;
   }

   if (currentGroupType_ != ProductGroupType::FXGroupType) {
      if (signingContainer_) {
         if (signingContainer_->isOffline()) {
            showHelp(tr("Signer is offline - settlement will not be possible"));
            return;
         }
         else {
            clearHelp();
         }
      }
      if (!curWallet_) {
         return;
      }
      if ((currentGroupType_ != ProductGroupType::XBTGroupType)
         && (getSelectedSide() != bs::network::Side::Sell) && !recvWallet_) {
         return;
      }
   }

   const double qty = getQuantity();
   const bool isBalanceOk = checkBalance(qty);

   if (!isBalanceOk) {
      ui_->labelBalanceValue->setFont(invalidBalanceFont_);
      return;
   }
   ui_->labelBalanceValue->setFont(QFont());

   if (qFuzzyIsNull(qty)) {
      return;
   }

  if ((currentGroupType_ == ProductGroupType::XBTGroupType) && authKey().empty()) {
     return;
  }

  showHelp({});
  ui_->pushButtonSubmit->setEnabled(true);
}

std::string RFQTicketXBT::mkRFQkey(const bs::network::RFQ &rfq)
{
   return rfq.security + "_" + rfq.product + "_" + std::to_string(rfq.side);
}

void RFQTicketXBT::putRFQ(const bs::network::RFQ &rfq)
{
   rfqMap_[mkRFQkey(rfq)] = rfq.quantity;
}

bool RFQTicketXBT::existsRFQ(const bs::network::RFQ &rfq)
{
   const auto rfqIt = rfqMap_.find(mkRFQkey(rfq));
   if (rfqIt == rfqMap_.end()) {
      return false;
   }
   return qFuzzyCompare(rfq.quantity, rfqIt->second);
}

bool RFQTicketXBT::eventFilter(QObject *watched, QEvent *evt)
{
   if (evt->type() == QEvent::KeyPress) {
      auto keyID = static_cast<QKeyEvent *>(evt)->key();
      if (ui_->pushButtonSubmit->isEnabled() && ((keyID == Qt::Key_Return) || (keyID == Qt::Key_Enter))) {
         submitButtonClicked();
      }
   }
   return QWidget::eventFilter(watched, evt);
}

double RFQTicketXBT::getQuantity() const
{
   const CustomDoubleValidator *validator = dynamic_cast<const CustomDoubleValidator*>(ui_->lineEditAmount->validator());
   if (validator == nullptr) {
      return 0;
   }
   return validator->GetValue(ui_->lineEditAmount->text());
}

void RFQTicketXBT::submitButtonClicked()
{
   bs::network::RFQ rfq;
   rfq.side = getSelectedSide();

   rfq.security = ui_->labelSecurityId->text().toStdString();
   rfq.product = getProduct().toStdString();

   if (rfq.security.empty() || rfq.product.empty()) {
      return;
   }

   saveLastSideSelection(rfq.product, rfq.security, getSelectedSide());

   rfq.quantity = getQuantity();

   if (qFuzzyIsNull(rfq.quantity)) {
      return;
   }
   if (existsRFQ(rfq)) {
      return;
   }
   putRFQ(rfq);

   auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());
   // just in case if 2 customers submit RFQ in exactly same ms
   rfq.requestId = "blocksettle:" + CryptoPRNG::generateRandom(8).toHexStr() +  std::to_string(timestamp.count());

  switch (currentGroupType_) {
   case ProductGroupType::GroupNotSelected:
      rfq.assetType = bs::network::Asset::Undefined;
      break;
   case ProductGroupType::FXGroupType:
      rfq.assetType = bs::network::Asset::SpotFX;
      break;
   case ProductGroupType::XBTGroupType:
      rfq.assetType = bs::network::Asset::SpotXBT;
      break;
   case ProductGroupType::CCGroupType:
      rfq.assetType = bs::network::Asset::PrivateMarket;
      break;
  }

   if (rfq.assetType == bs::network::Asset::SpotXBT) {
      rfq.requestorAuthPublicKey = authKey();
      if (rfq.requestorAuthPublicKey.empty()) {
         return;
      }
      transactionData_->SetFallbackRecvAddress(recvAddress());

      if ((rfq.side == bs::network::Side::Sell) && (rfq.product == bs::network::XbtCurrency)) {
         transactionData_->setMaxSpendAmount(maxAmount_);
         transactionData_->ReserveUtxosFor(rfq.quantity, rfq.requestId);
      }
   } else if (rfq.assetType == bs::network::Asset::PrivateMarket) {
      rfq.receiptAddress = recvAddress().display();

      if (rfq.side == bs::network::Side::Sell) {
         const auto wallet = transactionData_->getSigningWallet();
         const uint64_t spendVal = rfq.quantity * assetManager_->getCCLotSize(rfq.product);
         try {
            if (!ccCoinSel_) {
               throw std::runtime_error("CC coin selection is missing");
            }
            const auto &inputs = ccCoinSel_->GetSelectedTransactions();
            auto promAddr = std::make_shared<std::promise<bs::Address>>();
            auto futAddr = promAddr->get_future();
            const auto cbAddr = [&promAddr](const bs::Address &addr) {
               promAddr->set_value(addr);
            }; //TODO: refactor this
            wallet->getNewExtAddress(cbAddr);
            const auto txReq = wallet->createPartialTXRequest(spendVal, inputs, futAddr.get());
            rfq.coinTxInput = txReq.serializeState().toHexStr();
            utxoAdapter_->reserve(txReq, rfq.requestId);
         } catch (const std::exception &e) {
            BSMessageBox dlg(BSMessageBox::critical, tr("RFQ not sent"), QString::fromLatin1(e.what()));
            dlg.setWindowTitle(tr("RFQ Failure"));
            dlg.exec();
            return;
         }
      }
   }

   emit submitRFQ(rfq);
}

std::shared_ptr<TransactionData> RFQTicketXBT::GetTransactionData() const
{
   return transactionData_;
}

QPushButton* RFQTicketXBT::submitButton() const
{
   return ui_->pushButtonSubmit;
}

QLineEdit* RFQTicketXBT::lineEditAmount() const
{
   return ui_->lineEditAmount;
}

QPushButton* RFQTicketXBT::buyButton() const
{
   return ui_->pushButtonBuy;
}

QPushButton* RFQTicketXBT::sellButton() const
{
   return ui_->pushButtonSell;
}

QPushButton* RFQTicketXBT::numCcyButton() const
{
   return ui_->pushButtonNumCcy;
}

QPushButton* RFQTicketXBT::denomCcyButton() const
{
   return ui_->pushButtonDenomCcy;
}

bs::Address RFQTicketXBT::selectedAuthAddress() const
{
   return authAddressManager_->GetAddress(ui_->authenticationAddressComboBox->currentIndex());
}

double RFQTicketXBT::estimatedFee() const
{
   const auto wallet = getCurrentWallet();
   if (!wallet || !transactionData_) {
      return 0;
   }

   const auto balance = transactionData_->GetTransactionSummary().availableBalance;
   const auto maxVal = transactionData_->CalculateMaxAmount(
      bs::Address(CryptoPRNG::generateRandom(20), AddressEntryType_P2WPKH));
   if (maxVal <= 0) {
      return 0;
   }
   return (balance - maxVal);
}

void RFQTicketXBT::saveLastSideSelection(const std::string& product, const std::string& security, bs::network::Side::Type sideIndex)
{
   lastSideSelection_[product + security] = sideIndex;
}

bs::network::Side::Type RFQTicketXBT::getLastSideSelection(const std::string& product, const std::string& security)
{
   auto it = lastSideSelection_.find(product + security);
   if (it == lastSideSelection_.end()) {
      return bs::network::Side::Sell;
   }

   return it->second;
}

void RFQTicketXBT::disablePanel()
{
   resetTicket();

   // show help
   showHelp(tr("Login in order to send RFQ"));
}

void RFQTicketXBT::enablePanel()
{
   resetTicket();
   clearHelp();
}

void RFQTicketXBT::HideRFQControls()
{
   ui_->groupBoxSettlementInputs->setVisible(false);

   // amount and balance controls
   ui_->lineBeforeProduct->setVisible(false);
   ui_->verticalWidgetSelectedProduct->setVisible(false);

   ui_->lineBeforeBalance->setVisible(false);
   ui_->balanceLayout->setVisible(false);
}

void RFQTicketXBT::showHelp(const QString& helpText)
{
   ui_->helpLabel->setText(helpText);
   ui_->helpLabel->setVisible(true);
}

void RFQTicketXBT::clearHelp()
{
   ui_->helpLabel->setVisible(false);
}

void RFQTicketXBT::initProductGroupMap()
{
   groupNameToType_.emplace(bs::network::Asset::toString(bs::network::Asset::PrivateMarket)
      , ProductGroupType::CCGroupType);
   groupNameToType_.emplace(bs::network::Asset::toString(bs::network::Asset::SpotXBT)
      , ProductGroupType::XBTGroupType);
   groupNameToType_.emplace(bs::network::Asset::toString(bs::network::Asset::SpotFX)
      , ProductGroupType::FXGroupType);
}

RFQTicketXBT::ProductGroupType RFQTicketXBT::getProductGroupType(const QString& productGroup)
{
   auto it = groupNameToType_.find(productGroup.toStdString());
   if (it != groupNameToType_.end()) {
      return it->second;
   }

   return ProductGroupType::GroupNotSelected;
}

void RFQTicketXBT::onMaxClicked()
{
   auto balanceInfo = getBalanceInfo();

   switch(balanceInfo.productType) {
   case ProductGroupType::XBTGroupType:
   {
      const auto intBalance = std::floor(qMax<double>(balanceInfo.amount - estimatedFee(), 0) * BTCNumericTypes::BalanceDivider);
      ui_->lineEditAmount->setText(UiUtils::displayAmount(intBalance / BTCNumericTypes::BalanceDivider));
   }
      break;
   case ProductGroupType::CCGroupType:
      ui_->lineEditAmount->setText(UiUtils::displayCCAmount(qMax<double>(balanceInfo.amount, 0)));
      break;
   case ProductGroupType::FXGroupType:
      ui_->lineEditAmount->setText(UiUtils::displayCurrencyAmount(qMax<double>(balanceInfo.amount, 0)));
      break;
   }
   maxAmount_ = true;

   updateSubmitButton();
}

void RFQTicketXBT::onAmountEdited(const QString &)
{
   maxAmount_ = false;
   updateSubmitButton();
}

void RFQTicketXBT::SetCurrentIndicativePrices(const QString& bidPrice, const QString& offerPrice)
{
   if (bidPrice.isEmpty()) {
      currentBidPrice_ = EmptyInformationalLabelText;
   } else {
      currentBidPrice_ = bidPrice;
   }

   if (offerPrice.isEmpty()) {
      currentOfferPrice_ = EmptyInformationalLabelText;
   } else {
      currentOfferPrice_ = offerPrice;
   }
}

void RFQTicketXBT::updateIndicativePrice()
{
   auto selectedSide = getSelectedSide();
   if (selectedSide != bs::network::Side::Undefined) {
      int numCcySelected = ui_->pushButtonNumCcy->isChecked();
      bool isSell = numCcySelected ^ (selectedSide == bs::network::Side::Buy);

      if (isSell) {
         ui_->labelIndicativePrice->setText(currentBidPrice_);
      } else {
         ui_->labelIndicativePrice->setText(currentOfferPrice_);
      }
   } else {
      ui_->labelIndicativePrice->setText(EmptyInformationalLabelText);
   }
}

void RFQTicketXBT::onNumCcySelected()
{
   ui_->pushButtonNumCcy->setChecked(true);
   ui_->pushButtonDenomCcy->setChecked(false);

   currentProduct_ = ui_->pushButtonNumCcy->text();
   contraProduct_ = ui_->pushButtonDenomCcy->text();

   productSelectionChanged();
}

void RFQTicketXBT::onDenomCcySelected()
{
   ui_->pushButtonNumCcy->setChecked(false);
   ui_->pushButtonDenomCcy->setChecked(true);

   currentProduct_ = ui_->pushButtonDenomCcy->text();
   contraProduct_ = ui_->pushButtonNumCcy->text();

   productSelectionChanged();
}

void RFQTicketXBT::onSellSelected()
{
   ui_->pushButtonSell->setChecked(true);
   ui_->pushButtonBuy->setChecked(false);
   productSelectionChanged();
}

void RFQTicketXBT::onBuySelected()
{
   ui_->pushButtonSell->setChecked(false);
   ui_->pushButtonBuy->setChecked(true);
   productSelectionChanged();
}

void RFQTicketXBT::productSelectionChanged()
{
   rfqMap_.clear();
   setTransactionData();

   ui_->pushButtonSubmit->show();
   ui_->pushButtonCreateWallet->hide();

   ui_->lineEditAmount->setValidator(nullptr);
   ui_->lineEditAmount->setEnabled(false);
   ui_->lineEditAmount->clear();

   ui_->toolButtonMax->setEnabled(true);
   ui_->toolButtonXBTInputs->setEnabled(true);

   if (currentGroupType_ == ProductGroupType::FXGroupType) {
      ui_->lineEditAmount->setValidator(fxAmountValidator_);
      ui_->lineEditAmount->setEnabled(true);
   } else {
      bool canTradeXBT = (armory_->state() == ArmoryState::Ready)
         && signingContainer_
         && !signingContainer_->isOffline();

      ui_->lineEditAmount->setEnabled(canTradeXBT);
      ui_->toolButtonMax->setEnabled(canTradeXBT);
      ui_->toolButtonXBTInputs->setEnabled(canTradeXBT);

      if (!canTradeXBT) {
         ui_->labelBalanceValue->setText(tr("---"));
         return;
      }

      if (isXBTProduct()) {
         ui_->lineEditAmount->setValidator(xbtAmountValidator_);
      } else {
         if (currentGroupType_ == ProductGroupType::CCGroupType) {
            ui_->lineEditAmount->setValidator(ccAmountValidator_);

            ui_->comboBoxCCWallets->clear();

            const auto &product = getProduct();
            const auto ccWallet = getCCWallet(product.toStdString());
            if (ccWallet) {
               ui_->comboBoxCCWallets->addItem(QString::fromStdString(ccWallet->name()));
               ui_->comboBoxCCWallets->setItemData(0, QString::fromStdString(ccWallet->walletId()), UiUtils::WalletIdRole);
               ui_->comboBoxCCWallets->setCurrentIndex(0);

               ui_->comboBoxCCWallets->setEnabled(true);
               setCurrentCCWallet(ccWallet);
            } else {
               ui_->comboBoxCCWallets->setEnabled(false);
               setCurrentCCWallet(nullptr);

               if (signingContainer_ && !signingContainer_->isOffline() && walletsManager_) {
                  ui_->pushButtonSubmit->hide();
                  ui_->pushButtonCreateWallet->show();
                  ui_->pushButtonCreateWallet->setEnabled(true);
                  ui_->pushButtonCreateWallet->setText(tr("Create %1 wallet").arg(product));
               } else {
                  BSMessageBox errorMessage(BSMessageBox::critical, tr("Signer not connected")
                     , tr("Could not create CC subwallet.")
                     , this);
                  errorMessage.exec();
                  showHelp(tr("CC wallet missing"));
               }
            }
         } else {
            ui_->lineEditAmount->setValidator(fxAmountValidator_);
         }
      }
   }

   ui_->lineEditAmount->setFocus();

   updatePanel();
}

void RFQTicketXBT::onCreateWalletClicked()
{
   ui_->pushButtonCreateWallet->setEnabled(false);

   if (!walletsManager_->CreateCCLeaf(getProduct().toStdString())) {
      showHelp(tr("Create CC wallet request failed"));
   }
}
