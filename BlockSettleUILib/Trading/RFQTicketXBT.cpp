/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
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
#include "TradesUtils.h"
#include "TransactionData.h"
#include "TxClasses.h"
#include "UiUtils.h"
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
   connect(ui_->toolButtonXBTInputsSend, &QPushButton::clicked, this, &RFQTicketXBT::showCoinControl);
   connect(ui_->toolButtonMax, &QPushButton::clicked, this, &RFQTicketXBT::onMaxClicked);
   connect(ui_->comboBoxXBTWalletsRecv, qOverload<int>(&QComboBox::currentIndexChanged), this, &RFQTicketXBT::walletSelectedRecv);
   connect(ui_->comboBoxXBTWalletsSend, qOverload<int>(&QComboBox::currentIndexChanged), this, &RFQTicketXBT::walletSelectedSend);

   connect(ui_->pushButtonCreateWallet, &QPushButton::clicked, this, &RFQTicketXBT::onCreateWalletClicked);

   connect(ui_->lineEditAmount, &QLineEdit::textEdited, this, &RFQTicketXBT::onAmountEdited);

   connect(ui_->authenticationAddressComboBox, qOverload<int>(&QComboBox::currentIndexChanged), this, &RFQTicketXBT::onAuthAddrChanged);

   ui_->comboBoxXBTWalletsRecv->setEnabled(false);
   ui_->comboBoxXBTWalletsSend->setEnabled(false);

   disablePanel();
}

RFQTicketXBT::~RFQTicketXBT() = default;

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

   rfqMap_.clear();

   HideRFQControls();

   updatePanel();
}

std::map<UTXO, std::string> RFQTicketXBT::fixedXbtInputs() const
{
   if (!selectedXbtInputs_ || selectedXbtInputs_->UseAutoSel()) {
      return {};
   }
   return selectedXbtInputs_->getSelectedInputs();
}

void RFQTicketXBT::init(const std::shared_ptr<spdlog::logger> &logger, const std::shared_ptr<AuthAddressManager> &authAddressManager
   , const std::shared_ptr<AssetManager>& assetManager, const std::shared_ptr<QuoteProvider> &quoteProvider
   , const std::shared_ptr<SignContainer> &container, const std::shared_ptr<ArmoryConnection> &armory)
{
   logger_ = logger;
   authAddressManager_ = authAddressManager;
   assetManager_ = assetManager;
   signingContainer_ = container;
   armory_ = armory;

   if (signingContainer_) {
      connect(signingContainer_.get(), &SignContainer::ready, this, &RFQTicketXBT::onSignerReady);
   }

   updateSubmitButton();
}

std::shared_ptr<bs::sync::Wallet> RFQTicketXBT::getCCWallet(const std::string &cc) const
{
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
      const bool buyXBT = isXBTProduct() != (selectedSide == bs::network::Side::Sell);
      ui_->recAddressLayout->setVisible(buyXBT);
      ui_->XBTWalletLayoutRecv->setVisible(buyXBT);
      ui_->XBTWalletLayoutSend->setVisible(!buyXBT);
   }

   updateIndicativePrice();
   updateBalances();
   updateSubmitButton();
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

   ui_->pushButtonCreateWallet->hide();
   ui_->pushButtonCreateWallet->setText(tr("Create Wallet"));
   ui_->pushButtonSubmit->show();

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

   QString productToSpend = getProductToSpend();

   if (UiUtils::XbtCurrency == productToSpend) {
      balance.amount = getXbtBalance().GetValueBitcoin();
      balance.product = UiUtils::XbtCurrency;
      balance.productType = ProductGroupType::XBTGroupType;
   } else {
      if (currentGroupType_ == ProductGroupType::CCGroupType) {
         auto ccWallet = getCCWallet(getProduct().toStdString());
         balance.amount = ccWallet ? ccWallet->getSpendableBalance() : 0;
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
   connect(walletsManager_.get(), &bs::sync::WalletsManager::walletAdded, this, &RFQTicketXBT::walletsLoaded);
   connect(walletsManager_.get(), &bs::sync::WalletsManager::walletDeleted, this, &RFQTicketXBT::walletsLoaded);

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
   if (!signingContainer_ || !walletsManager_ || walletsManager_->hdWallets().empty()) {
      return;
   }

   ui_->comboBoxXBTWalletsRecv->clear();
   ui_->comboBoxXBTWalletsSend->clear();

   if (signingContainer_->isOffline()) {
      ui_->comboBoxXBTWalletsRecv->setEnabled(false);
      ui_->comboBoxXBTWalletsSend->setEnabled(false);
   } else {
      ui_->comboBoxXBTWalletsRecv->setEnabled(true);
      ui_->comboBoxXBTWalletsSend->setEnabled(true);

      // Only full wallets could be used to send, recv could be also done with WO
      UiUtils::fillHDWalletsComboBox(ui_->comboBoxXBTWalletsRecv, walletsManager_, UiUtils::WoWallets::Enable);
      UiUtils::fillHDWalletsComboBox(ui_->comboBoxXBTWalletsSend, walletsManager_, UiUtils::WoWallets::Disable);
   }

   productSelectionChanged();
}

void RFQTicketXBT::onSignerReady()
{
   updateSubmitButton();
   ui_->receivingWalletWidgetRecv->setEnabled(!signingContainer_->isOffline());
   ui_->receivingWalletWidgetSend->setEnabled(!signingContainer_->isOffline());
}

void RFQTicketXBT::fillRecvAddresses()
{
   auto recvWallet = getRecvXbtWallet();
   if (recvWallet) {
      UiUtils::fillRecvAddressesComboBoxHDWallet(ui_->receivingAddressComboBox, recvWallet);
   }
}

void RFQTicketXBT::showCoinControl()
{
   if (!selectedXbtInputs_) {
      SPDLOG_LOGGER_ERROR(logger_, "selectedXbtInputs_ is not set");
      return;
   }

   int rc = CoinControlDialog(selectedXbtInputs_, true, this).exec();
   if (rc == QDialog::Accepted) {
      updateBalances();
      updateSubmitButton();
   }
}

void RFQTicketXBT::walletSelectedRecv(int index)
{
   productSelectionChanged();
}

void RFQTicketXBT::walletSelectedSend(int index)
{
   productSelectionChanged();
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
   authAddr_ = authAddressManager_->GetAddress(authAddressManager_->FromVerifiedIndex(index));
   authKey_.clear();
   if (authAddr_.isNull()) {
      return;
   }
   const auto settlLeaf = authAddressManager_->getSettlementLeaf(authAddr_);

   const auto &cbPubKey = [this](const SecureBinaryData &pubKey) {
      authKey_ = pubKey.toHexStr();
      QMetaObject::invokeMethod(this, &RFQTicketXBT::updateSubmitButton);
   };

   if (settlLeaf) {
      settlLeaf->getRootPubkey(cbPubKey);
   }
   else {
      walletsManager_->createSettlementLeaf(authAddr_, cbPubKey);
   }
}

void RFQTicketXBT::setSubmitRFQ(RFQTicketXBT::SubmitRFQCb submitRFQCb)
{
   submitRFQCb_ = std::move(submitRFQCb);
}

bs::Address RFQTicketXBT::recvXbtAddress() const
{
   auto recvWallet = getRecvXbtWallet();
   const auto index = ui_->receivingAddressComboBox->currentIndex();
   if ((index < 0) || !recvWallet) {
      return bs::Address();
   }

   if (index != 0) {
      return bs::Address::fromAddressString(ui_->receivingAddressComboBox->currentText().toStdString());
   }

   auto leaves = recvWallet->getGroup(recvWallet->getXBTGroupType())->getLeaves();
   if (leaves.empty()) {
      return bs::Address();
   }
   auto promAddr = std::make_shared<std::promise<bs::Address>>();
   auto futAddr = promAddr->get_future();
   const auto &cbAddr = [promAddr](const bs::Address &addr) {
      promAddr->set_value(addr);
   };
   leaves.front()->getNewExtAddress(cbAddr);
   return futAddr.get();
}

bool RFQTicketXBT::checkBalance(double qty) const
{
   if (getSelectedSide() == bs::network::Side::Buy) {
      return true;
   }
   const auto balance = getBalanceInfo();
   return (qty <= balance.amount);
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

      if (getProductToSpend() == UiUtils::XbtCurrency && !getSendXbtWallet()) {
         return;
      }

      if (getProductToRecv() == UiUtils::XbtCurrency && !getRecvXbtWallet()) {
         return;
      }

      if (currentGroupType_ == ProductGroupType::CCGroupType) {
         auto ccWallet = getCCWallet(getProduct().toStdString());
         if (!ccWallet) {
            return;
         }
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
   auto rfq = std::make_shared<bs::network::RFQ>();
   rfq->side = getSelectedSide();

   rfq->security = ui_->labelSecurityId->text().toStdString();
   rfq->product = getProduct().toStdString();

   if (rfq->security.empty() || rfq->product.empty()) {
      return;
   }

   saveLastSideSelection(rfq->product, rfq->security, getSelectedSide());

   rfq->quantity = getQuantity();

   if (qFuzzyIsNull(rfq->quantity)) {
      return;
   }
   if (existsRFQ(*rfq)) {
      return;
   }
   putRFQ(*rfq);

   auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());
   // just in case if 2 customers submit RFQ in exactly same ms
   rfq->requestId = "blocksettle:" + CryptoPRNG::generateRandom(8).toHexStr() +  std::to_string(timestamp.count());

  switch (currentGroupType_) {
   case ProductGroupType::GroupNotSelected:
      rfq->assetType = bs::network::Asset::Undefined;
      break;
   case ProductGroupType::FXGroupType:
      rfq->assetType = bs::network::Asset::SpotFX;
      break;
   case ProductGroupType::XBTGroupType:
      rfq->assetType = bs::network::Asset::SpotXBT;
      break;
   case ProductGroupType::CCGroupType:
      rfq->assetType = bs::network::Asset::PrivateMarket;
      break;
  }

   if (rfq->assetType == bs::network::Asset::SpotXBT) {
      rfq->requestorAuthPublicKey = authKey();
      if (rfq->requestorAuthPublicKey.empty()) {
         return;
      }
   } else if (rfq->assetType == bs::network::Asset::PrivateMarket) {
      auto ccWallet = getCCWallet(rfq->product);
      if (!ccWallet) {
         SPDLOG_LOGGER_ERROR(logger_, "can't find CC wallet for {}", rfq->product);
         return;
      }

      if (rfq->side == bs::network::Side::Sell) {
         rfq->receiptAddress = recvXbtAddress().display();

         const uint64_t spendVal = rfq->quantity * assetManager_->getCCLotSize(rfq->product);
         if (!ccWallet) {
            SPDLOG_LOGGER_ERROR(logger_, "ccWallet is not set");
            return;
         }

         const auto ccInputsCb = [this, spendVal, rfq, ccWallet]
            (const std::vector<UTXO> &ccInputs) mutable
         {
            QMetaObject::invokeMethod(this, [this, spendVal, rfq, ccInputs, ccWallet] {
               uint64_t inputVal = 0;
               for (const auto &input : ccInputs) {
                  inputVal += input.getValue();
               }
               if (inputVal < spendVal) {
                  BSMessageBox(BSMessageBox::critical, tr("RFQ not sent")
                     , tr("Insufficient input amount")).exec();
                  return;
               }

               const auto cbAddr = [this, spendVal, rfq, ccInputs, ccWallet](const bs::Address &addr) {
                  try {
                     const auto txReq = ccWallet->createPartialTXRequest(spendVal, ccInputs, addr);
                     rfq->coinTxInput = txReq.serializeState().toHexStr();
                     auto reservationToken = bs::UtxoReservationToken::makeNewReservation(logger_, txReq, rfq->requestId);
                     submitRFQCb_(*rfq, std::move(reservationToken));
                  }
                  catch (const std::exception &e) {
                     BSMessageBox(BSMessageBox::critical, tr("RFQ Failure")
                        , QString::fromLatin1(e.what()), this).exec();
                     return;
                  }
               };
               if (inputVal == spendVal) {
                  cbAddr({});
               }
               else {
                  ccWallet->getNewExtAddress(cbAddr);
               }
            });
         };
         bool result = ccWallet->getSpendableTxOutList(ccInputsCb, spendVal);
         if (!result) {
            SPDLOG_LOGGER_ERROR(logger_, "can't spendable TX list");
         }
         return;
      } else {
         auto promAddr = std::promise<bs::Address>();
         auto cbAddr = [&promAddr](const bs::Address &addr) {
            promAddr.set_value(addr);
         };
         ccWallet->getNewExtAddress(cbAddr);
         rfq->receiptAddress = promAddr.get_future().get().display();
      }
   }

   submitRFQCb_(*rfq, bs::UtxoReservationToken{});
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
   return authAddr_;
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

std::shared_ptr<bs::sync::hd::Wallet> RFQTicketXBT::xbtWallet() const
{
   if (getProductToSpend() == UiUtils::XbtCurrency) {
      return getSendXbtWallet();
   }
   if (getProductToRecv() == UiUtils::XbtCurrency) {
      return getRecvXbtWallet();
   }
   return nullptr;
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
         const auto xbtWallet = getSendXbtWallet();
         if (!xbtWallet) {
            ui_->lineEditAmount->clear();
            updateSubmitButton();
            return;
         }

         auto cb = [this](const std::map<UTXO, std::string> &inputs) {
            QMetaObject::invokeMethod(this, [this, inputs] {
               std::vector<UTXO> utxos;
               utxos.reserve(inputs.size());
               for (const auto &input : inputs) {
                  utxos.emplace_back(std::move(input.first));
               }
               auto feeCb = [this, utxos = std::move(utxos)](float fee) {
                  QMetaObject::invokeMethod(this, [this, fee, utxos = std::move(utxos)] {
                     float feePerByte = ArmoryConnection::toFeePerByte(fee);
                     uint64_t total = 0;
                     for (const auto &utxo : utxos) {
                        total += utxo.getValue();
                     }
                     const uint64_t fee = bs::tradeutils::estimatePayinFeeWithoutChange(utxos, feePerByte);
                     const double spendableQuantity = std::max(0.0, (total - fee) / BTCNumericTypes::BalanceDivider);
                     ui_->lineEditAmount->setText(UiUtils::displayAmount(spendableQuantity));
                     updateSubmitButton();
                  });
               };
               armory_->estimateFee(bs::tradeutils::feeTargetBlockCount(), feeCb);
            });
         };

         auto inputs = fixedXbtInputs();
         if (!inputs.empty()) {
            cb(inputs);
         } else {
            auto leaves = xbtWallet->getGroup(xbtWallet->getXBTGroupType())->getLeaves();
            auto xbtWallets = std::vector<std::shared_ptr<bs::sync::Wallet>>(leaves.begin(), leaves.end());
            bs::tradeutils::getSpendableTxOutList(xbtWallets, cb);
         }
         return;
      }
      case ProductGroupType::CCGroupType: {
         ui_->lineEditAmount->setText(UiUtils::displayCCAmount(qMax<double>(balanceInfo.amount, 0)));
         break;
      }
      case ProductGroupType::FXGroupType: {
         ui_->lineEditAmount->setText(UiUtils::displayCurrencyAmount(qMax<double>(balanceInfo.amount, 0)));
         break;
      }
   }

   updateSubmitButton();
}

void RFQTicketXBT::onAmountEdited(const QString &)
{
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

   ui_->pushButtonSubmit->show();
   ui_->pushButtonCreateWallet->hide();

   ui_->lineEditAmount->setValidator(nullptr);
   ui_->lineEditAmount->setEnabled(false);
   ui_->lineEditAmount->clear();

   ui_->toolButtonMax->setEnabled(true);
   ui_->toolButtonXBTInputsSend->setEnabled(true);

   selectedXbtInputs_.reset();

   if (currentGroupType_ == ProductGroupType::FXGroupType) {
      ui_->lineEditAmount->setValidator(fxAmountValidator_);
      ui_->lineEditAmount->setEnabled(true);
   } else {
      bool canTradeXBT = (armory_->state() == ArmoryState::Ready)
         && signingContainer_
         && !signingContainer_->isOffline();

      ui_->lineEditAmount->setEnabled(canTradeXBT);
      ui_->toolButtonMax->setEnabled(canTradeXBT);
      ui_->toolButtonXBTInputsSend->setEnabled(canTradeXBT);

      if (!canTradeXBT) {
         ui_->labelBalanceValue->setText(tr("---"));
         return;
      }

      if (isXBTProduct()) {
         ui_->lineEditAmount->setValidator(xbtAmountValidator_);
      } else {
         if (currentGroupType_ == ProductGroupType::CCGroupType) {
            ui_->lineEditAmount->setValidator(ccAmountValidator_);

            const auto &product = getProduct();
            const auto ccWallet = getCCWallet(product.toStdString());
            if (!ccWallet) {
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

   const auto xbtWallet = getSendXbtWallet();
   if (xbtWallet) {
      selectedXbtInputs_ = std::make_shared<SelectedTransactionInputs>(xbtWallet->getGroup(xbtWallet->getXBTGroupType())
         , false, true);
   }

   ui_->lineEditAmount->setFocus();

   updatePanel();

   fillRecvAddresses();
}

std::shared_ptr<bs::sync::hd::Wallet> RFQTicketXBT::getSendXbtWallet() const
{
   if (!walletsManager_) {
      return nullptr;
   }
   return walletsManager_->getHDWalletById(ui_->comboBoxXBTWalletsSend->currentData(UiUtils::WalletIdRole).toString().toStdString());
}

std::shared_ptr<bs::sync::hd::Wallet> RFQTicketXBT::getRecvXbtWallet() const
{
   if (!walletsManager_) {
      return nullptr;
   }
   return walletsManager_->getHDWalletById(ui_->comboBoxXBTWalletsRecv->currentData(UiUtils::WalletIdRole).toString().toStdString());
}

bs::XBTAmount RFQTicketXBT::getXbtBalance() const
{
   const auto fixedInputs = fixedXbtInputs();
   if (!fixedInputs.empty()) {
      uint64_t sum = 0;
      for (const auto &utxo : fixedInputs) {
         sum += utxo.first.getValue();
      }
      return bs::XBTAmount(sum);
   }

   auto xbtWallet = getSendXbtWallet();
   if (!xbtWallet) {
      return {};
   }

   double sum = 0;
   for (const auto &leave : xbtWallet->getGroup(xbtWallet->getXBTGroupType())->getLeaves()) {
      sum += leave->getSpendableBalance();
   }
   return bs::XBTAmount(sum);
}

QString RFQTicketXBT::getProductToSpend() const
{
   if (getSelectedSide() == bs::network::Side::Sell) {
      return currentProduct_;
   } else {
      return contraProduct_;
   }
}

QString RFQTicketXBT::getProductToRecv() const
{
   if (getSelectedSide() == bs::network::Side::Buy) {
      return currentProduct_;
   } else {
      return contraProduct_;
   }
}

void RFQTicketXBT::onCreateWalletClicked()
{
   ui_->pushButtonCreateWallet->setEnabled(false);

   if (!walletsManager_->CreateCCLeaf(getProduct().toStdString())) {
      showHelp(tr("Create CC wallet request failed"));
   }
}
