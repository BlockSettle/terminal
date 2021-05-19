/*

***********************************************************************************
* Copyright (C) 2019 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "OTCWindowsAdapterBase.h"
#include "OTCWindowsManager.h"

#include "UiUtils.h"
#include "Wallets/SyncWalletsManager.h"
#include "Wallets/SyncHDWallet.h"
#include "AuthAddressManager.h"
#include "AssetManager.h"
#include "CoinControlDialog.h"
#include "SelectedTransactionInputs.h"
#include "TradesUtils.h"
#include "UtxoReservationManager.h"
#include "XBTAmount.h"
#include "BSMessageBox.h"

#include <QComboBox>
#include <QLabel>
#include <QProgressBar>

namespace {
   const std::chrono::milliseconds kTimerRepeatTimeMSec{ 500 };
   const QString secondsRemaining = QObject::tr("second(s) remaining");
}

OTCWindowsAdapterBase::OTCWindowsAdapterBase(QWidget* parent /*= nullptr*/)
   : QWidget(parent)
{
   connect(&timeoutTimer_, &QTimer::timeout, this, &OTCWindowsAdapterBase::onUpdateTimerData);
   timeoutTimer_.setInterval(kTimerRepeatTimeMSec);
}

void OTCWindowsAdapterBase::setChatOTCManager(const std::shared_ptr<OTCWindowsManager>& otcManager)
{
   otcManager_ = otcManager;
   connect(otcManager_.get(), &OTCWindowsManager::syncInterfaceRequired, this, [this]() {
      onSyncInterface();
   });

   connect(otcManager_.get(), &OTCWindowsManager::updateMDDataRequired, this,
      [this](bs::network::Asset::Type type, const QString& security, const bs::network::MDFields& fields)
   {
      onUpdateMD(type, security, fields);
   });

   connect(otcManager_.get(), &OTCWindowsManager::updateBalances, this, [this]() {
      onUpdateBalances();
   });
}

std::shared_ptr<bs::sync::WalletsManager> OTCWindowsAdapterBase::getWalletManager() const
{
   return otcManager_->getWalletManager();
}

std::shared_ptr<AuthAddressManager> OTCWindowsAdapterBase::getAuthManager() const
{
   return otcManager_->getAuthManager();
}

std::shared_ptr<AssetManager> OTCWindowsAdapterBase::getAssetManager() const
{
   return otcManager_->getAssetManager();
}

std::shared_ptr<bs::UTXOReservationManager> OTCWindowsAdapterBase::getUtxoManager() const
{
   return otcManager_->getUtxoManager();
}

void OTCWindowsAdapterBase::setPeer(const bs::network::otc::Peer &)
{
}

bs::UtxoReservationToken OTCWindowsAdapterBase::releaseReservation()
{
   return std::move(reservation_);
}

void OTCWindowsAdapterBase::setReservation(bs::UtxoReservationToken&& reservation)
{
   reservation_ = std::move(reservation);
}

void OTCWindowsAdapterBase::onSyncInterface()
{
}

void OTCWindowsAdapterBase::onUpdateMD(bs::network::Asset::Type type, const QString& security, const bs::network::MDFields& fields)
{
   if (productGroup_ != type || security_ != security) {
      return;
   }

   updateIndicativePrices(type, security, fields);

   // overloaded in direved class
   onMDUpdated();
}

void OTCWindowsAdapterBase::onMDUpdated()
{
}

void OTCWindowsAdapterBase::onUpdateBalances()
{
}

void OTCWindowsAdapterBase::showXBTInputsClicked(QComboBox *walletsCombobox)
{
   reservation_.release();
   showXBTInputs(walletsCombobox);
}

void OTCWindowsAdapterBase::showXBTInputs(QComboBox *walletsCombobox)
{
   const bool useAutoSel = selectedUTXO_.empty();


   const auto &hdWallet = getCurrentHDWalletFromCombobox(walletsCombobox);

   std::vector<UTXO> allUTXOs;
   if (!hdWallet->canMixLeaves()) {
      auto purpose = UiUtils::getSelectedHwPurpose(walletsCombobox);
      allUTXOs = getUtxoManager()->getAvailableXbtUTXOs(hdWallet->walletId(), purpose, bs::UTXOReservationManager::kIncludeZcOtc);
   }
   else {
      allUTXOs = getUtxoManager()->getAvailableXbtUTXOs(hdWallet->walletId(), bs::UTXOReservationManager::kIncludeZcOtc);
   }

   auto inputs = std::make_shared<SelectedTransactionInputs>(allUTXOs);

   // Set this to false is needed otherwise current selection would be cleared
   inputs->SetUseAutoSel(useAutoSel);

   if (!useAutoSel) {
      for (const auto &utxo : selectedUTXO_) {
         inputs->SetUTXOSelection(utxo.getTxHash(), utxo.getTxOutIndex());
      }
   }

   CoinControlDialog dialog(inputs, true, this);
   int rc = dialog.exec();
   if (rc != QDialog::Accepted) {
      emit xbtInputsProcessed();
      return;
   }

   auto selectedInputs = dialog.selectedInputs();
   if (bs::UtxoReservation::instance()->containsReservedUTXO(selectedInputs)) {
      BSMessageBox(BSMessageBox::critical, tr("UTXO reservation failed"),
         tr("Some of selected UTXOs has been already reserved"), this).exec();
      showXBTInputs(walletsCombobox);
      return;
   }
   selectedUTXO_ = std::move(selectedInputs);
   if (!selectedUTXO_.empty()) {
      reservation_ = getUtxoManager()->makeNewReservation(selectedUTXO_);
   }

   emit xbtInputsProcessed();
}

void OTCWindowsAdapterBase::onUpdateTimerData()
{
   if (!currentTimeoutData_.progressBarTimeLeft_ || !currentTimeoutData_.labelTimeLeft_) {
      timeoutTimer_.stop();
      return;
   }

   const auto currentOfferEndTimestamp = currentTimeoutData_.offerTimestamp_ + std::chrono::seconds(timeoutSec_);
   const auto diff = currentOfferEndTimestamp - std::chrono::steady_clock::now();
   const auto diffSeconds = std::chrono::duration_cast<std::chrono::seconds>(diff);

   currentTimeoutData_.labelTimeLeft_->setText(QString(QLatin1String("%1 %2")).arg(diffSeconds.count()).arg(secondsRemaining));
   currentTimeoutData_.progressBarTimeLeft_->setMaximum(timeoutSec_.count());
   currentTimeoutData_.progressBarTimeLeft_->setValue(diffSeconds.count());

   if (diffSeconds.count() < 0) {
      timeoutTimer_.stop();
   }
}

void OTCWindowsAdapterBase::updateIndicativePrices(bs::network::Asset::Type type, const QString& security
   , const bs::network::MDFields& fields)
{
   for (const auto &field : fields) {
      switch (field.type) {
      case bs::network::MDField::PriceBid:
         sellIndicativePrice_ = field.value;
         break;
      case bs::network::MDField::PriceOffer:
         buyIndicativePrice_ = field.value;
         break;
      default:  break;
      }
   }
}

BTCNumericTypes::balance_type OTCWindowsAdapterBase::getXBTSpendableBalanceFromCombobox(QComboBox *walletsCombobox) const
{
   const auto hdWallet = getCurrentHDWalletFromCombobox(walletsCombobox);
   if (!hdWallet) {
      return .0;
   }

   BTCNumericTypes::balance_type totalBalance{};
   if (selectedUTXO_.empty()) {
      BTCNumericTypes::satoshi_type sum = 0;
      if (!hdWallet->canMixLeaves()) {
         auto purpose = UiUtils::getSelectedHwPurpose(walletsCombobox);
         sum = getUtxoManager()->getAvailableXbtUtxoSum(hdWallet->walletId(), purpose, bs::UTXOReservationManager::kIncludeZcOtc);
      }
      else {
         sum = getUtxoManager()->getAvailableXbtUtxoSum(hdWallet->walletId(), bs::UTXOReservationManager::kIncludeZcOtc);
      }

      return bs::XBTAmount(sum).GetValueBitcoin();
   }
   else {
      for (const auto &utxo : selectedUTXO_) {
         totalBalance += bs::XBTAmount((int64_t)utxo.getValue()).GetValueBitcoin();
      }
   }

   return totalBalance;
}

std::shared_ptr<bs::sync::hd::Wallet> OTCWindowsAdapterBase::getCurrentHDWalletFromCombobox(QComboBox *walletsCombobox) const
{
   const auto walletId = walletsCombobox->currentData(UiUtils::WalletIdRole).toString().toStdString();
   return getWalletManager()->getHDWalletById(walletId);
}

void OTCWindowsAdapterBase::submitProposal(QComboBox *walletsCombobox, bs::XBTAmount amount,  CbSuccess onSuccess)
{
   const auto hdWallet = getCurrentHDWalletFromCombobox(walletsCombobox);
   if (!hdWallet) {
      return;
   }

   auto cbUtxoSet = [caller = QPointer<OTCWindowsAdapterBase>(this), cbSuccess = std::move(onSuccess)](std::vector<UTXO>&& utxos) {
      if (!caller) {
         return;
      }

      caller->setSelectedInputs(utxos);
      caller->setReservation(caller->getUtxoManager()->makeNewReservation(utxos));

      cbSuccess();
   };

   auto checkAmount = bs::UTXOReservationManager::CheckAmount::Enabled;

   if (!hdWallet->canMixLeaves()) {
      auto purpose = UiUtils::getSelectedHwPurpose(walletsCombobox);
      getUtxoManager()->getBestXbtUtxoSet(hdWallet->walletId(), purpose, amount.GetValue()
         , std::move(cbUtxoSet), true, checkAmount);
   }
   else {
      getUtxoManager()->getBestXbtUtxoSet(hdWallet->walletId(), amount.GetValue()
         , std::move(cbUtxoSet), true, checkAmount, bs::UTXOReservationManager::kIncludeZcOtc);
   }
}

QString OTCWindowsAdapterBase::getXBTRange(bs::network::otc::Range xbtRange)
{
   return QStringLiteral("%1 - %2")
      .arg(UiUtils::displayCurrencyAmount(xbtRange.lower))
      .arg(UiUtils::displayCurrencyAmount(xbtRange.upper));
}

QString OTCWindowsAdapterBase::getCCRange(bs::network::otc::Range ccRange)
{
   return QStringLiteral("%1 - %2")
      .arg(UiUtils::displayCurrencyAmount(bs::network::otc::fromCents(ccRange.lower)))
      .arg(UiUtils::displayCurrencyAmount(bs::network::otc::fromCents(ccRange.upper)));
}

QString OTCWindowsAdapterBase::getSide(bs::network::otc::Side requestSide, bool isOwnRequest)
{
   if (!isOwnRequest) {
      requestSide = bs::network::otc::switchSide(requestSide);
   }

   return QString::fromStdString(bs::network::otc::toString(requestSide));
}

void OTCWindowsAdapterBase::clearSelectedInputs()
{
   selectedUTXO_.clear();
   reservation_ = {};
}

void OTCWindowsAdapterBase::setupTimer(TimeoutData&& timeoutData)
{
   currentTimeoutData_ = std::move(timeoutData);
   onUpdateTimerData();
   timeoutTimer_.start();
}

std::chrono::seconds OTCWindowsAdapterBase::getSeconds(std::chrono::milliseconds durationInMillisecs)
{
   return std::chrono::duration_cast<std::chrono::seconds>(durationInMillisecs);
}

void OTCWindowsAdapterBase::setSelectedInputs(const std::vector<UTXO>& selectedUTXO)
{
   selectedUTXO_ = selectedUTXO;

}
