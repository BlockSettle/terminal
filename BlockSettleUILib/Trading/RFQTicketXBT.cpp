/*

***********************************************************************************
* Copyright (C) 2019 - 2020, BlockSettle AB
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
#include <spdlog/spdlog.h>
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
#include "TxClasses.h"
#include "UiUtils.h"
#include "Wallets/SyncHDWallet.h"
#include "Wallets/SyncWalletsManager.h"
#include "XbtAmountValidator.h"
#include "UtxoReservationManager.h"
#include "UtxoReservation.h"
#include "TradeSettings.h"

#include <cstdlib>


namespace {
   static const QString kEmptyInformationalLabelText = QString::fromStdString("--");
}


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

   connect(ui_->authenticationAddressComboBox, qOverload<int>(&QComboBox::currentIndexChanged)
      , this, &RFQTicketXBT::onAuthAddrChanged);

   ui_->comboBoxXBTWalletsRecv->setEnabled(false);
   ui_->comboBoxXBTWalletsSend->setEnabled(false);

   disablePanel();
}

RFQTicketXBT::~RFQTicketXBT() = default;

void RFQTicketXBT::resetTicket()
{
   ui_->labelProductGroup->setText(kEmptyInformationalLabelText);
   ui_->labelSecurityId->setText(kEmptyInformationalLabelText);
   ui_->labelIndicativePrice->setText(kEmptyInformationalLabelText);

   currentBidPrice_ = kEmptyInformationalLabelText;
   currentOfferPrice_ = kEmptyInformationalLabelText;
   currentGroupType_ = ProductGroupType::GroupNotSelected;

   ui_->lineEditAmount->setValidator(nullptr);
   ui_->lineEditAmount->setEnabled(false);
   ui_->lineEditAmount->clear();

   rfqMap_.clear();

   HideRFQControls();

   updatePanel();
}

bs::FixedXbtInputs RFQTicketXBT::fixedXbtInputs()
{
   return std::move(fixedXbtInputs_);
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
      const bool buyXBT = getProductToRecv() == UiUtils::XbtCurrency;
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
   if (!assetManager_ && balances_.empty()) {
      return balance;
   }

   QString productToSpend = getProductToSpend();

   if (UiUtils::XbtCurrency == productToSpend) {
      balance.amount = getXbtBalance().GetValueBitcoin();
      balance.product = UiUtils::XbtCurrency;
      balance.productType = ProductGroupType::XBTGroupType;
   } else {
      if (currentGroupType_ == ProductGroupType::CCGroupType) {
         balance.amount = utxoReservationManager_->getAvailableCCUtxoSum(getProduct().toStdString());
         balance.product = productToSpend;
         balance.productType = ProductGroupType::CCGroupType;
      } else {
         const double divisor = std::pow(10, UiUtils::GetAmountPrecisionFX());
         try {
            const double bal = assetManager_ ?
               assetManager_->getBalance(productToSpend.toStdString(), bs::UTXOReservationManager::kIncludeZcRequestor, nullptr)
               : balances_.at(productToSpend.toStdString());
            balance.amount = std::floor(bal * divisor) / divisor;
            balance.product = productToSpend;
            balance.productType = ProductGroupType::FXGroupType;
         }
         catch (const std::exception &) {}
      }
   }
   return balance;
}

QString RFQTicketXBT::getProduct() const
{
   return currentProduct_;
}

void RFQTicketXBT::init(const std::shared_ptr<spdlog::logger>& logger)
{
   logger_ = logger;
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

      UiUtils::fillHDWalletsComboBox(ui_->comboBoxXBTWalletsRecv, walletsManager_, UiUtils::WalletsTypes::All);
      // CC does not support to send from hardware wallets
      int sendWalletTypes = (currentGroupType_ == ProductGroupType::CCGroupType) ?
               UiUtils::WalletsTypes::Full : (UiUtils::WalletsTypes::Full | UiUtils::WalletsTypes::HardwareSW);
      UiUtils::fillHDWalletsComboBox(ui_->comboBoxXBTWalletsSend, walletsManager_, sendWalletTypes);

      const auto walletId = walletsManager_->getDefaultSpendWalletId();

      auto selected = UiUtils::selectWalletInCombobox(ui_->comboBoxXBTWalletsSend, walletId, static_cast<UiUtils::WalletsTypes>(sendWalletTypes));
      if (selected == -1) {
         auto primaryWallet = walletsManager_->getPrimaryWallet();
         if (primaryWallet != nullptr) {
            UiUtils::selectWalletInCombobox(ui_->comboBoxXBTWalletsSend, primaryWallet->walletId(), static_cast<UiUtils::WalletsTypes>(sendWalletTypes));
         }
      }

      selected = UiUtils::selectWalletInCombobox(ui_->comboBoxXBTWalletsRecv, walletId, UiUtils::WalletsTypes::All);
      if (selected == -1) {
         auto primaryWallet = walletsManager_->getPrimaryWallet();
         if (primaryWallet != nullptr) {
            UiUtils::selectWalletInCombobox(ui_->comboBoxXBTWalletsRecv, primaryWallet->walletId(), UiUtils::WalletsTypes::All);
         }
      }
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
   if (walletsManager_) {
      auto recvWallet = getRecvXbtWallet();
      if (recvWallet) {
         if (!recvWallet->canMixLeaves()) {
            auto xbtGroup = recvWallet->getGroup(recvWallet->getXBTGroupType());
            auto purpose = UiUtils::getSelectedHwPurpose(ui_->comboBoxXBTWalletsRecv);
            UiUtils::fillRecvAddressesComboBox(ui_->receivingAddressComboBox, { xbtGroup->getLeaf(purpose) });
         } else {
            UiUtils::fillRecvAddressesComboBoxHDWallet(ui_->receivingAddressComboBox, recvWallet, true);
         }
      }
   }
   else {
      const auto& walletId = ui_->comboBoxXBTWalletsRecv->currentData(UiUtils::WalletIdRole).toString().toStdString();
      emit needWalletData(walletId);
   }
}

bool RFQTicketXBT::preSubmitCheck()
{
   if (currentGroupType_ == ProductGroupType::XBTGroupType) {
      if ((!authAddressManager_ && !authAddr_.empty())
         || (authAddressManager_->GetState(authAddr_) == AuthAddressManager::AuthAddressState::Verified)) {
         return true;
      }

      const auto qty = getQuantity();
      assert(tradeSettings_);

      bool validAmount = false;
      if (currentProduct_ == UiUtils::XbtCurrency) {
         validAmount = tradeSettings_->xbtTier1Limit > bs::XBTAmount(qty).GetValue();
      } else {
         const double indPrice = getIndicativePrice();
         bs::XBTAmount price(indPrice * (1.0 + (tradeSettings_->xbtPriceBand / 100.0)));
         validAmount = price > bs::XBTAmount(qty);
      }

      if (!validAmount) {
         auto amountStr = UiUtils::displayQuantity(bs::XBTAmount((int64_t)tradeSettings_->xbtTier1Limit).GetValueBitcoin()
            , bs::network::XbtCurrency);
         BSMessageBox(BSMessageBox::info
            , tr("Notice"), tr("Authentication Address not verified")
            , tr("Trades above %1 are not permitted for non-verified Authentication Addresses. "
                 "To verify your Authentication Address, execute three trades below the %2 threshold, "
                 "and BlockSettle will validate the address during its next cycle.").arg(amountStr).arg(amountStr), this).exec();
         return false;
      }
   }

   return true;
}

void RFQTicketXBT::showCoinControl()
{
   const auto xbtWallet = getSendXbtWallet();
   if (!xbtWallet) {
      SPDLOG_LOGGER_ERROR(logger_, "can't find XBT wallet");
      return;
   }
   auto walletId = xbtWallet->walletId();
   ui_->toolButtonXBTInputsSend->setEnabled(false);

   // Need to release current reservation to be able select them back
   fixedXbtInputs_.utxoRes.release();

   std::vector<UTXO> utxos;
   if (!xbtWallet->canMixLeaves()) {
      auto purpose = UiUtils::getSelectedHwPurpose(ui_->comboBoxXBTWalletsSend);
      utxos = utxoReservationManager_->getAvailableXbtUTXOs(xbtWallet->walletId(), purpose, bs::UTXOReservationManager::kIncludeZcRequestor);
   }
   else {
      utxos = utxoReservationManager_->getAvailableXbtUTXOs(xbtWallet->walletId(), bs::UTXOReservationManager::kIncludeZcRequestor);
   }

   ui_->toolButtonXBTInputsSend->setEnabled(true);
   const bool useAutoSel = fixedXbtInputs_.inputs.empty();

   auto inputs = std::make_shared<SelectedTransactionInputs>(utxos);
   // Set this to false is needed otherwise current selection would be cleared
   inputs->SetUseAutoSel(useAutoSel);
   for (const auto &utxo : fixedXbtInputs_.inputs) {
      inputs->SetUTXOSelection(utxo.first.getTxHash(), utxo.first.getTxOutIndex());
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

   fixedXbtInputs_.inputs.clear();
   for (const auto &selectedInput : selectedInputs) {
      fixedXbtInputs_.inputs.emplace(selectedInput, walletId);
   }

   if (!selectedInputs.empty()) {
      fixedXbtInputs_.utxoRes = utxoReservationManager_->makeNewReservation(selectedInputs);
   }

   updateBalances();
   updateSubmitButton();
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

   walletsLoaded();
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

void RFQTicketXBT::sendDeferredRFQs()
{
   if (authKey_.empty() || deferredRFQs_.empty()) {
      return;
   }
   decltype(deferredRFQs_) tmpRFQs;
   tmpRFQs.swap(deferredRFQs_);
   logger_->debug("[RFQTicketXBT::sendDeferredRFQs] {} RFQ[s]", tmpRFQs.size());

   for (const auto& id : tmpRFQs) {
      sendRFQ(id);
   }
}

void RFQTicketXBT::onSettlLeavesLoaded(unsigned int)
{
   if (authKey_.empty()) {
      if (authAddr_.empty()) {
         logger_->warn("[RFQTicketXBT::onSettlLeavesLoaded] no default auth address");
         return;
      }
      const auto &cbPubKey = [this](const SecureBinaryData &pubKey) {
         authKey_ = pubKey.toHexStr();
         sendDeferredRFQs();
      };
      const auto settlLeaf = walletsManager_->getSettlementLeaf(authAddr_);
      if (!settlLeaf) {
         logger_->warn("[RFQTicketXBT::onSettlLeavesLoaded] no settlement leaf"
            " for auth address {}", authAddr_.display());
         return;
      }
      settlLeaf->getRootPubkey(cbPubKey);
   }
   else {
      sendDeferredRFQs();
   }
}

std::string RFQTicketXBT::authKey() const
{
   return authKey_;
}

void RFQTicketXBT::onAuthAddrChanged(int index)
{
   auto addressString = ui_->authenticationAddressComboBox->itemText(index).toStdString();
   if (addressString.empty()) {
      return;
   }

   authAddr_ = bs::Address::fromAddressString(addressString);

   if (walletsManager_) {
      authKey_.clear();
      const auto settlLeaf = walletsManager_->getSettlementLeaf(authAddr_);

      const auto& cbPubKey = [this](const SecureBinaryData& pubKey) {
         authKey_ = pubKey.toHexStr();
         QMetaObject::invokeMethod(this, &RFQTicketXBT::updateSubmitButton);
      };

      if (settlLeaf) {
         settlLeaf->getRootPubkey(cbPubKey);
      } else {
         walletsManager_->createSettlementLeaf(authAddr_, cbPubKey);
      }
   }
   else {
      emit needAuthKey(authAddr_);
   }
}

void RFQTicketXBT::onUTXOReservationChanged(const std::string& walletId)
{
   logger_->debug("[RFQTicketXBT::onUTXOReservationChanged] walletId='{}'", walletId);
   if (walletId.empty()) {
      updateBalances();
      updateSubmitButton();
      return;
   }

   auto xbtWallet = getSendXbtWallet();
   if (xbtWallet && (walletId == xbtWallet->walletId() || xbtWallet->getLeaf(walletId))) {
      updateBalances();
   }
}

void RFQTicketXBT::setSubmitRFQ(RFQTicketXBT::SubmitRFQCb submitRFQCb)
{
   submitRFQCb_ = std::move(submitRFQCb);
}

void RFQTicketXBT::setCancelRFQ(RFQTicketXBT::CancelRFQCb cb)
{
   cancelRFQCb_ = std::move(cb);
}

bs::Address RFQTicketXBT::recvXbtAddressIfSet() const
{
   const auto index = ui_->receivingAddressComboBox->currentIndex();
   if (index < 0) {
      SPDLOG_LOGGER_ERROR(logger_, "invalid address index");
      return bs::Address();
   }

   if (index == 0) {
      // Automatic address generation
      return bs::Address();
   }

   bs::Address address;
   const auto &addressStr = ui_->receivingAddressComboBox->currentText().toStdString();
   try {
      address = bs::Address::fromAddressString(addressStr);
   } catch (const std::exception &e) {
      SPDLOG_LOGGER_ERROR(logger_, "can't parse address '{}': {}", addressStr, e.what());
      return address;
   }

   // Sanity checks
   auto recvWallet = getRecvXbtWallet();
   if (!recvWallet) {
      SPDLOG_LOGGER_ERROR(logger_, "recv XBT wallet is not set");
      return bs::Address();
   }
   auto wallet = walletsManager_->getWalletByAddress(address);
   if (!wallet) {
      SPDLOG_LOGGER_ERROR(logger_, "can't find receiving wallet for address {}", address.display());
      return bs::Address();
   }
   auto hdWallet = walletsManager_->getHDRootForLeaf(wallet->walletId());
   if (!hdWallet) {
      SPDLOG_LOGGER_ERROR(logger_, "can't find HD wallet for receiving wallet {}", wallet->walletId());
      return bs::Address();
   }
   if (hdWallet != recvWallet) {
      SPDLOG_LOGGER_ERROR(logger_, "receiving HD wallet {} does not contain waller {}", hdWallet->walletId(), wallet->walletId());
      return bs::Address();
   }

   return address;
}

bool RFQTicketXBT::checkBalance(double qty) const
{
   const auto balance = getBalanceInfo();
   if (getSelectedSide() == bs::network::Side::Buy) {
      if (currentGroupType_ == ProductGroupType::CCGroupType) {
         return balance.amount >= getXbtReservationAmountForCc(qty, getOfferPrice()).GetValueBitcoin();
      }
      return (balance.amount > 0);
   }
   else {
      return (qty <= balance.amount);
   }
}

bool RFQTicketXBT::checkAuthAddr() const
{
   if (!ui_->authenticationAddressComboBox->isVisible()) {
      return true;
   }
   else if (ui_->authenticationAddressComboBox->count() == 0) {
      return false;
   }

   if (authAddressManager_) {
      if (authAddressManager_->GetState(authAddr_) == AuthAddressManager::AuthAddressState::Verified) {
         return true;
      }

      const auto& tradeSettings = authAddressManager_->tradeSettings();
      if (!tradeSettings) {
         return false;
      }
   }
   else {
      //TODO: implement more thorough checking if needed
      return (!authAddr_.empty());
   }

   return true;
}

void RFQTicketXBT::updateSubmitButton()
{
   ui_->pushButtonSubmit->setEnabled(false);

   if (!assetManager_ && signingContainer_) {
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

      if (getProductToSpend() == UiUtils::XbtCurrency) {
         if (walletsManager_ && !getSendXbtWallet()) {
            return;
         }
         else if (!hasSendXbtWallet()) {
            return;
         }
      }

      if (getProductToRecv() == UiUtils::XbtCurrency) {
         if (walletsManager_ && !getRecvXbtWallet()) {
            return;
         }
         else if (!hasRecvXbtWallet()) {
            return;
         }
      }

      if (currentGroupType_ == ProductGroupType::CCGroupType) {
         if (walletsManager_) {
            auto ccWallet = getCCWallet(getProduct().toStdString());
            if (!ccWallet) {
               return;
            }
         }
         else {
            if (!hasCCWallet()) {
               return;
            }
         }
      }
   }

   const double qty = getQuantity();
   if (qFuzzyIsNull(qty)) {
      return;
   }

   const bool isBalanceOk = checkBalance(qty);
   const bool isAuthOk = checkAuthAddr();

   if (!isBalanceOk || !isAuthOk) {
      ui_->labelBalanceValue->setFont(invalidBalanceFont_);
      return;
   }
   ui_->labelBalanceValue->setFont(QFont());

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

double RFQTicketXBT::getOfferPrice() const
{
   const CustomDoubleValidator *validator = dynamic_cast<const CustomDoubleValidator*>(ui_->lineEditAmount->validator());
   if (validator == nullptr) {
      return 0;
   }
   return validator->GetValue(currentOfferPrice_);
}

void RFQTicketXBT::submitButtonClicked()
{
   if (!preSubmitCheck()) {
      return;
   }

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

   if (currentGroupType_ == ProductGroupType::XBTGroupType) {
      if (utxoReservationManager_) {
         auto minXbtAmount = bs::tradeutils::minXbtAmount(utxoReservationManager_->feeRatePb());
         if (expectedXbtAmountMin().GetValue() < minXbtAmount.GetValue()) {
            auto minAmountStr = UiUtils::displayQuantity(minXbtAmount.GetValueBitcoin(), bs::network::XbtCurrency);
            BSMessageBox(BSMessageBox::critical, tr("Spot XBT"), tr("Invalid amount")
               , tr("Expected bitcoin amount will not cover network fee.\nMinimum amount: %1").arg(minAmountStr), this).exec();
            return;
         }
      }
   }

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

   const auto &rfqId = CryptoPRNG::generateRandom(8).toHexStr();
   pendingRFQs_[rfqId] = rfq;

   if (!existsRFQ(*rfq)) {
      putRFQ(*rfq);
      sendRFQ(rfqId);
   }
}

void RFQTicketXBT::onSendRFQ(const std::string &id, const QString &symbol, double amount, bool buy)
{
   auto rfq = std::make_shared<bs::network::RFQ>();
   rfq->side = buy ? bs::network::Side::Buy : bs::network::Side::Sell;

   const CurrencyPair cp(symbol.toStdString());
   rfq->security = symbol.toStdString();
   rfq->product = cp.NumCurrency();
   rfq->quantity = amount;
   if (assetManager_) {
      rfq->assetType = assetManager_->GetAssetTypeForSecurity(rfq->security);
   }
   else {
      try {
         rfq->assetType = assetTypes_.at(rfq->security);
      }
      catch (const std::exception&) {
         logger_->error("[{}] no asset type found for {}", __func__, rfq->security);
         return;
      }
   }

   if (rfq->security.empty() || rfq->product.empty() || qFuzzyIsNull(rfq->quantity)) {
      return;
   }

   pendingRFQs_[id] = rfq;

   if (rfq->assetType == bs::network::Asset::SpotXBT) {
      if (authAddressManager_) {
         authAddr_ = authAddressManager_->getDefault();
      }
      if (authAddr_.empty()) {
         deferredRFQs_.push_back(id);
         return;
      }
      if (walletsManager_) {
         if (!walletsManager_->getSettlementLeaf(authAddr_)) {
            deferredRFQs_.push_back(id);
            return;
         }
      }
   }

   sendRFQ(id);
}

void RFQTicketXBT::sendRFQ(const std::string &id)
{
   const auto &itRFQ = pendingRFQs_.find(id);
   if (itRFQ == pendingRFQs_.end()) {
      logger_->error("[RFQTicketXBT::onSendRFQ] RFQ with id {} not found", id);
      return;
   }
   logger_->debug("[RFQTicketXBT::sendRFQ] sending RFQ {}", id);

   auto rfq = itRFQ->second;

   if (rfq->requestId.empty()) {
      rfq->requestId = id;
   }

   if (rfq->assetType == bs::network::Asset::SpotXBT) {
      rfq->requestorAuthPublicKey = authKey();
      if (rfq->requestorAuthPublicKey.empty()) {
         logger_->debug("[RFQTicketXBT::onSendRFQ] auth key is empty for {}", authAddr_.display());
         deferredRFQs_.push_back(id);
      }
      else {
         reserveBestUtxoSetAndSubmit(id, rfq);
      }
      return;
   }
   else if (rfq->assetType == bs::network::Asset::PrivateMarket) {
      auto ccWallet = getCCWallet(rfq->product);
      if (!ccWallet) {
         SPDLOG_LOGGER_ERROR(logger_, "can't find CC wallet for {}", rfq->product);
         return;
      }

      if (rfq->side == bs::network::Side::Sell) {
         // Sell
         const auto &recvXbtAddressCb = [this, id, rfq, ccWallet]
            (const bs::Address &recvXbtAddr)
         {
            rfq->receiptAddress = recvXbtAddr.display();
            const uint64_t spendVal = rfq->quantity * assetManager_->getCCLotSize(rfq->product);
            if (!ccWallet) {
               SPDLOG_LOGGER_ERROR(logger_, "ccWallet is not set");
               return;
            }

            const auto ccInputsCb = [this, id, spendVal, rfq, ccWallet]
               (const std::vector<UTXO> &ccInputs) mutable
            {
               QMetaObject::invokeMethod(this, [this, id, spendVal, rfq, ccInputs, ccWallet] {
                  uint64_t inputVal = 0;
                  for (const auto &input : ccInputs) {
                     inputVal += input.getValue();
                  }
                  if (inputVal < spendVal) {
                     // This should not normally happen!
                     SPDLOG_LOGGER_ERROR(logger_, "insufficient input amount: {}, expected: {}, requestId: {}", inputVal, spendVal, rfq->requestId);
                     BSMessageBox(BSMessageBox::critical, tr("RFQ not sent")
                        , tr("Insufficient input amount")).exec();
                     return;
                  }

                  const auto cbAddr = [this, spendVal, id, rfq, ccInputs, ccWallet](const bs::Address &addr)
                  {
                     try {
                        const auto txReq = ccWallet->createPartialTXRequest(
                           spendVal, ccInputs
                           , { addr, RECIP_GROUP_CHANG_1 } //change to group 1 (cc group)
                           , 0, {}, {}
                           /*
                           This cc is created without recipients. Set the assumed recipient count
                           to 1 so the coin selection algo can run, otherwise all presented inputs
                           will be selected, which is wasteful.

                           The assumed recipient count isn't relevant to the fee calculation on
                           the cc side of the tx since only the xbt side covers network fees.
                           */
                           , 1);

                        auto resolveCB = [this, id, rfq]
                           (bs::error::ErrorCode result, const Codec_SignerState::SignerState &state)
                        {
                           if (result != bs::error::ErrorCode::NoError) {
                              std::stringstream ss;
                              ss << "failed to resolve CC half signer with error code: " << (int)result;
                              throw std::runtime_error(ss.str());
                           }

                           bs::core::wallet::TXSignRequest req;
                           req.armorySigner_.deserializeState(state);
                           auto reservationToken = utxoReservationManager_->makeNewReservation(
                              req.getInputs(nullptr), rfq->requestId);

                           rfq->coinTxInput = BinaryData::fromString(state.SerializeAsString()).toHexStr();
                           submitRFQCb_(id, *rfq, std::move(reservationToken));
                        };
                        signingContainer_->resolvePublicSpenders(txReq, resolveCB);
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
                     ccWallet->getNewChangeAddress(cbAddr);
                  }
               });
            };
            bool result = ccWallet->getSpendableTxOutList(ccInputsCb, spendVal, true);
            if (!result) {
               SPDLOG_LOGGER_ERROR(logger_, "can't spendable TX list");
            }
         };

         auto recvXbtAddrIfSet = recvXbtAddressIfSet();
         if (recvXbtAddrIfSet.isValid()) {
            recvXbtAddressCb(recvXbtAddrIfSet);
         }
         else {
            auto recvXbtWallet = getRecvXbtWallet();
            if (!recvXbtWallet) {
               SPDLOG_LOGGER_ERROR(logger_, "recv XBT wallet is not set");
               return;
            }
            auto leaves = recvXbtWallet->getGroup(recvXbtWallet->getXBTGroupType())->getLeaves();
            if (leaves.empty()) {
               SPDLOG_LOGGER_ERROR(logger_, "can't find XBT leaves");
               return;
            }
            // BST-2474: All addresses related to trading, not just change addresses, should use internal addresses
            leaves.front()->getNewIntAddress(recvXbtAddressCb);
         }
         return;
      }

      // Buy
      auto cbRecvAddr = [this, id, rfq](const bs::Address &recvAddr) {
         rfq->receiptAddress = recvAddr.display();
         reserveBestUtxoSetAndSubmit(id, rfq);
      };
      // BST-2474: All addresses related to trading, not just change addresses, should use internal addresses.
      // This has no effect as CC wallets have only external addresses.
      // But CCLeaf::getSpendableTxOutList is require only 1 conf so it's OK.
      ccWallet->getNewIntAddress(cbRecvAddr);
      return;
   }

   submitRFQCb_(id, *rfq, bs::UtxoReservationToken{});
   pendingRFQs_.erase(itRFQ);
}

void RFQTicketXBT::onCancelRFQ(const std::string &id)
{
   const auto &itRFQ = pendingRFQs_.find(id);
   if (itRFQ == pendingRFQs_.end()) {
      logger_->error("[RFQTicketXBT::onCancelRFQ] failed to find RFQ {}", id);
      return;
   }
   logger_->debug("[RFQTicketXBT::onCancelRFQ] cancelling RFQ {}", id);
   if (cancelRFQCb_) {
      cancelRFQCb_(id);
   }
   pendingRFQs_.erase(itRFQ);
}

void RFQTicketXBT::onMDUpdate(bs::network::Asset::Type, const QString &security, bs::network::MDFields mdFields)
{
   auto &mdInfo = mdInfo_[security.toStdString()];
   mdInfo.merge(bs::network::MDField::get(mdFields));
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

UiUtils::WalletsTypes RFQTicketXBT::xbtWalletType() const
{
   QComboBox* combobox = nullptr;
   if (getProductToSpend() == UiUtils::XbtCurrency) {
      combobox = ui_->comboBoxXBTWalletsSend;
   }
   if (getProductToRecv() == UiUtils::XbtCurrency) {
      combobox = ui_->comboBoxXBTWalletsRecv;
   }

   if (!combobox) {
      return UiUtils::None;
   }

   return UiUtils::getSelectedWalletType(combobox);
}

void RFQTicketXBT::onParentAboutToHide()
{
   fixedXbtInputs_ = {};
}

void RFQTicketXBT::onVerifiedAuthAddresses(const std::vector<bs::Address>& addrs)
{
   if (addrs.empty()) {
      return;
   }
   UiUtils::fillAuthAddressesComboBoxWithSubmitted(ui_->authenticationAddressComboBox, addrs);
   onAuthAddrChanged(ui_->authenticationAddressComboBox->currentIndex());
}

void RFQTicketXBT::onBalance(const std::string& currency, double balance)
{
   balances_[currency] = balance;
   updateBalances();
}

void RFQTicketXBT::onWalletBalance(const bs::sync::WalletBalanceData& wbd)
{
   balances_[wbd.id] = wbd.balSpendable;
   updateBalances();
}

void RFQTicketXBT::onHDWallet(const bs::sync::HDWalletData& wallet)
{
   const auto &it = std::find_if(wallets_.cbegin(), wallets_.cend()
      , [wallet](const bs::sync::HDWalletData &w) { return (wallet.id == w.id); });
   if (it == wallets_.end()) {
      wallets_.push_back(wallet);
   }
   else {
      wallets_.emplace(it, wallet);
   }
   UiUtils::fillHDWalletsComboBox(ui_->comboBoxXBTWalletsRecv, wallets_, UiUtils::WalletsTypes::All);
   // CC does not support to send from hardware wallets
   int sendWalletTypes = (currentGroupType_ == ProductGroupType::CCGroupType) ?
      UiUtils::WalletsTypes::Full : (UiUtils::WalletsTypes::Full | UiUtils::WalletsTypes::HardwareSW);
   UiUtils::fillHDWalletsComboBox(ui_->comboBoxXBTWalletsSend, wallets_, sendWalletTypes);
}

void RFQTicketXBT::onWalletData(const std::string& walletId, const bs::sync::WalletData& wd)
{
   if (ui_->comboBoxXBTWalletsRecv->currentData(UiUtils::WalletIdRole).toString().toStdString() == walletId) {
      UiUtils::fillRecvAddressesComboBoxHDWallet(ui_->receivingAddressComboBox, { wd });
   }
}

void RFQTicketXBT::onAuthKey(const bs::Address& addr, const BinaryData& authKey)
{
   if (addr == authAddr_) {
      const auto& authKeyHex = authKey.toHexStr();
      if (authKey_ != authKeyHex) {
         logger_->debug("[{}] got auth key: {}", __func__, authKeyHex);
         authKey_ = authKeyHex;
         updateSubmitButton();
      }
      sendDeferredRFQs();
   }
}

void RFQTicketXBT::onTradeSettings(const std::shared_ptr<bs::TradeSettings>& ts)
{
   tradeSettings_ = ts;
}

void RFQTicketXBT::onReservedUTXOs(const std::string& resId
   , const std::string& subId, const std::vector<UTXO>& utxos)
{
   const auto& it = pendingRFQs_.find(resId);
   if (it == pendingRFQs_.end()) {
      return;
   }
   logger_->debug("[{}] sending RFQ {} after {} UTXOs reservation", __func__
      , resId, utxos.size());
   if (utxos.empty()) {
      logger_->error("[{}] failed to reserve {}/{}", resId, subId);
   }
   else {
      submitRFQCb_(resId, *it->second, {});
   }
   pendingRFQs_.erase(it);
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

         std::vector<UTXO> utxos;
         if (!fixedXbtInputs_.inputs.empty()) {
            utxos.reserve(fixedXbtInputs_.inputs.size());
            for (const auto &utxoPair : fixedXbtInputs_.inputs) {
               utxos.push_back(utxoPair.first);
            }
         }
         else {
            if (!xbtWallet->canMixLeaves()) {
               auto purpose = UiUtils::getSelectedHwPurpose(ui_->comboBoxXBTWalletsSend);
               utxos = utxoReservationManager_->getAvailableXbtUTXOs(xbtWallet->walletId(), purpose, bs::UTXOReservationManager::kIncludeZcRequestor);
            }
            else {
               utxos = utxoReservationManager_->getAvailableXbtUTXOs(xbtWallet->walletId(), bs::UTXOReservationManager::kIncludeZcRequestor);
            }
         }

         auto feeCb = [this, utxos = std::move(utxos)](float fee) {
            QMetaObject::invokeMethod(this, [this, fee, utxos = std::move(utxos)]{
               float feePerByteArmory = ArmoryConnection::toFeePerByte(fee);
               auto feePerByte = std::max(feePerByteArmory, utxoReservationManager_->feeRatePb());
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
      currentBidPrice_ = kEmptyInformationalLabelText;
   } else {
      currentBidPrice_ = bidPrice;
   }

   if (offerPrice.isEmpty()) {
      currentOfferPrice_ = kEmptyInformationalLabelText;
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
      ui_->labelIndicativePrice->setText(kEmptyInformationalLabelText);
   }
}

double RFQTicketXBT::getIndicativePrice() const
{
   const auto &mdIt = mdInfo_.find(ui_->labelSecurityId->text().toStdString());
   if (mdIt == mdInfo_.end()) {
      return false;
   }

   auto selectedSide = getSelectedSide();
   if (selectedSide == bs::network::Side::Undefined) {
      return .0;
   }
   bool numCcySelected = ui_->pushButtonNumCcy->isChecked();
   bool isSell = selectedSide == bs::network::Side::Buy
      ? !numCcySelected
      : numCcySelected;

   if (isSell) {
      return mdIt->second.bidPrice;
   }
   else {
      return mdIt->second.askPrice;
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

   fixedXbtInputs_ = {};

   if (currentGroupType_ == ProductGroupType::FXGroupType) {
      ui_->lineEditAmount->setValidator(fxAmountValidator_);
      ui_->lineEditAmount->setEnabled(true);
   } else {
      bool canTradeXBT = false;
      if (armory_ && signingContainer_) {
         canTradeXBT = (armory_->state() == ArmoryState::Ready)
            && !signingContainer_->isOffline();
      }
      else {
         canTradeXBT = true;
      }

      ui_->lineEditAmount->setEnabled(canTradeXBT);
      ui_->toolButtonMax->setEnabled(canTradeXBT);
      ui_->toolButtonXBTInputsSend->setEnabled(canTradeXBT);

      if (!canTradeXBT) {
         ui_->labelBalanceValue->setText(tr("---"));
         return;
      }

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
         if (currentProduct_ == UiUtils::XbtCurrency) {
            ui_->lineEditAmount->setValidator(xbtAmountValidator_);
         } else {
            ui_->lineEditAmount->setValidator(fxAmountValidator_);
         }
      }
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
   auto wallet = walletsManager_->getHDWalletById(ui_->comboBoxXBTWalletsSend->
      currentData(UiUtils::WalletIdRole).toString().toStdString());
   if (!wallet) {
      const auto &defaultWallet = walletsManager_->getDefaultWallet();
      if (defaultWallet) {
         wallet = walletsManager_->getHDRootForLeaf(defaultWallet->walletId());
      }
   }
   return wallet;
}

std::shared_ptr<bs::sync::hd::Wallet> RFQTicketXBT::getRecvXbtWallet() const
{
   if (!walletsManager_) {
      return nullptr;
   }
   auto wallet = walletsManager_->getHDWalletById(ui_->comboBoxXBTWalletsRecv->
      currentData(UiUtils::WalletIdRole).toString().toStdString());
   if (!wallet && walletsManager_->getDefaultWallet()) {
      wallet = walletsManager_->getHDRootForLeaf(walletsManager_->getDefaultWallet()->walletId());
   }
   return wallet;
}

std::string RFQTicketXBT::getXbtLeafId(const std::string& hdWalletId) const
{
   const auto& it = std::find_if(wallets_.cbegin(), wallets_.cend()
      , [hdWalletId](const bs::sync::HDWalletData& wd) { return (wd.id == hdWalletId); });
   if (it == wallets_.end()) {
      for (const auto& wallet : wallets_) {
         for (const auto& group : wallet.groups) {
            if ((group.type != bs::hd::CoinType::Bitcoin_main) && (group.type != bs::hd::Bitcoin_test)) {
               continue;
            }
            for (const auto& leaf : group.leaves) {
               if (std::find_if(leaf.ids.cbegin(), leaf.ids.cend()
                  , [hdWalletId](const std::string& id) { return (id == hdWalletId); })
                  != leaf.ids.end()) {
                  return hdWalletId;
               }
            }
         }
      }
      return {};
   }
   for (const auto& group : it->groups) {
      if ((group.type != bs::hd::CoinType::Bitcoin_main) && (group.type != bs::hd::Bitcoin_test)) {
         continue;
      }
      if (!group.leaves.empty()) {
         const auto& leaf = *group.leaves.cbegin();
         return *leaf.ids.cbegin();
      }
   }
   return {};
}

bool RFQTicketXBT::hasSendXbtWallet() const
{
   return !wallets_.empty();  //TODO: implement more thorough checking
}

bool RFQTicketXBT::hasRecvXbtWallet() const
{
   return !wallets_.empty();  //TODO: implement more thorough checking
}

bs::XBTAmount RFQTicketXBT::getXbtBalance() const
{
   const auto &fixedInputs = fixedXbtInputs_.inputs;
   if (!fixedXbtInputs_.inputs.empty()) {
      int64_t sum = 0;
      for (const auto &utxo : fixedInputs) {
         sum += utxo.first.getValue();
      }
      return bs::XBTAmount(sum);
   }

   if (walletsManager_) {
      auto xbtWallet = getSendXbtWallet();
      if (!xbtWallet) {
         return bs::XBTAmount(0.0);
      }

      if (!xbtWallet->canMixLeaves()) {
         auto purpose = UiUtils::getSelectedHwPurpose(ui_->comboBoxXBTWalletsSend);
         return bs::XBTAmount(utxoReservationManager_->getAvailableXbtUtxoSum(
            xbtWallet->walletId(), purpose, bs::UTXOReservationManager::kIncludeZcRequestor));
      } else {
         return bs::XBTAmount(utxoReservationManager_->getAvailableXbtUtxoSum(
            xbtWallet->walletId(), bs::UTXOReservationManager::kIncludeZcRequestor));
      }
   }
   else {
      const auto &walletId = ui_->comboBoxXBTWalletsSend->currentData(UiUtils::WalletIdRole).toString().toStdString();
      const auto& xbtLeafId = getXbtLeafId(walletId);
      if (xbtLeafId.empty()) {
         return bs::XBTAmount(0.0); //TODO: use default XBT leaf
      }
      try {
         return bs::XBTAmount(balances_.at(xbtLeafId));
      }
      catch (const std::exception&) {
         logger_->error("[{}] balance for leaf {} not found", __func__, xbtLeafId);
         return bs::XBTAmount(0.0);
      }
   }
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

bs::XBTAmount RFQTicketXBT::expectedXbtAmountMin() const
{
   if (currentProduct_ == UiUtils::XbtCurrency) {
      return bs::XBTAmount(getQuantity());
   }
   const auto &tradeSettings = authAddressManager_->tradeSettings();
   auto maxPrice = getIndicativePrice() * (1 + (tradeSettings->xbtPriceBand / 100));
   return bs::XBTAmount(getQuantity() / maxPrice);
}

bs::XBTAmount RFQTicketXBT::getXbtReservationAmountForCc(double quantity, double offerPrice) const
{
   return bs::XBTAmount(quantity * offerPrice * bs::tradeutils::reservationQuantityMultiplier());
}

void RFQTicketXBT::reserveBestUtxoSetAndSubmit(const std::string &id
   , const std::shared_ptr<bs::network::RFQ>& rfq)
{
   if (walletsManager_) {
      // Skip UTXO reservations amount checks for buy fiat requests as reserved XBT amount is 20% more than expected from current price.
      auto checkAmount = (rfq->side == bs::network::Side::Sell && rfq->product != bs::network::XbtCurrency) ?
          bs::UTXOReservationManager::CheckAmount::Enabled : bs::UTXOReservationManager::CheckAmount::Disabled;

      const auto &submitRFQWrapper = [rfqTicket = QPointer<RFQTicketXBT>(this), id, rfq]
      {
         if (!rfqTicket) {
            return;
         }
         rfqTicket->submitRFQCb_(id, *rfq, std::move(rfqTicket->fixedXbtInputs_.utxoRes));
      };
      auto getWalletAndReserve = [rfqTicket = QPointer<RFQTicketXBT>(this), submitRFQWrapper, checkAmount]
         (BTCNumericTypes::satoshi_type amount, bool partial)
      {
         auto cbBestUtxoSet = [rfqTicket, submitRFQWrapper](bs::FixedXbtInputs&& fixedXbt) {
            if (!rfqTicket) {
               return;
            }
            rfqTicket->fixedXbtInputs_ = std::move(fixedXbt);
            submitRFQWrapper();
         };

         auto hdWallet = rfqTicket->getSendXbtWallet();
         if (!hdWallet->canMixLeaves()) {
            auto purpose = UiUtils::getSelectedHwPurpose(rfqTicket->ui_->comboBoxXBTWalletsSend);
            rfqTicket->utxoReservationManager_->reserveBestXbtUtxoSet(
               hdWallet->walletId(), purpose, amount,
               partial, std::move(cbBestUtxoSet), true, checkAmount);
         } else {
            rfqTicket->utxoReservationManager_->reserveBestXbtUtxoSet(
               hdWallet->walletId(), amount,
               partial, std::move(cbBestUtxoSet), true, checkAmount, bs::UTXOReservationManager::kIncludeZcRequestor);
         }
      };

      if (!fixedXbtInputs_.inputs.empty()) {
         submitRFQWrapper();
         return; // already reserved by user
      }

      auto quantity = bs::XBTAmount(rfq->quantity).GetValue();
      if (rfq->side == bs::network::Side::Buy) {
         if (rfq->assetType == bs::network::Asset::PrivateMarket) {
            quantity *= bs::XBTAmount(getOfferPrice()).GetValue();
         } else if (rfq->assetType == bs::network::Asset::SpotXBT) {
            quantity /= getOfferPrice();
         }
      }

      const bool partial = rfq->assetType == bs::network::Asset::PrivateMarket;
      getWalletAndReserve(quantity, partial);
   }
   else {
      const auto& walletId = ui_->comboBoxXBTWalletsSend->currentData(UiUtils::WalletIdRole).toString().toStdString();
      if (rfq->assetType == bs::network::Asset::PrivateMarket
         && rfq->side == bs::network::Side::Buy) {
         auto maxXbtQuantity = getXbtReservationAmountForCc(rfq->quantity, getOfferPrice()).GetValue();
         emit needReserveUTXOs(id, walletId, maxXbtQuantity, true);
      }
      else {
         if ((rfq->side == bs::network::Side::Sell && rfq->product != bs::network::XbtCurrency) ||
            (rfq->side == bs::network::Side::Buy && rfq->product == bs::network::XbtCurrency)) {
            submitRFQCb_(id, *rfq, {}); // already reserved
            pendingRFQs_.erase(id);
         }
         else if (!fixedXbtInputs_.inputs.empty()) {
            submitRFQCb_(id, *rfq, {}); // already reserved
            pendingRFQs_.erase(id);
         }

         auto quantity = bs::XBTAmount(rfq->quantity).GetValue();
         if (rfq->side == bs::network::Side::Buy) {
            if (rfq->assetType == bs::network::Asset::PrivateMarket) {
               quantity *= bs::XBTAmount(getOfferPrice()).GetValue();
            } else if (rfq->assetType == bs::network::Asset::SpotXBT) {
               quantity /= getOfferPrice();
            }
         }
         const bool partial = rfq->assetType == bs::network::Asset::PrivateMarket;
         emit needReserveUTXOs(id, walletId, quantity, partial);
      }
   }
}

void RFQTicketXBT::onCreateWalletClicked()
{
   ui_->pushButtonCreateWallet->setEnabled(false);

   if (!walletsManager_->CreateCCLeaf(getProduct().toStdString())) {
      showHelp(tr("Create CC wallet request failed"));
   }
}
