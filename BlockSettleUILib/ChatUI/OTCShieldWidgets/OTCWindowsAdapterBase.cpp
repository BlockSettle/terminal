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

void OTCWindowsAdapterBase::setPeer(const bs::network::otc::Peer &)
{
}

void OTCWindowsAdapterBase::onAboutToApply()
{
}

void OTCWindowsAdapterBase::onChatRoomChanged()
{
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
   auto cb = [handle = validityFlag_.handle(), this](const std::map<UTXO, std::string> &utxos) mutable {
      ValidityGuard guard(handle);
      if (!handle.isValid()) {
         return;
      }
      allUTXOs_.clear();
      allUTXOs_.reserve(utxos.size());
      for (const auto &utxo : utxos) {
         allUTXOs_.push_back(utxo.first);
      }
      QMetaObject::invokeMethod(this, &OTCWindowsAdapterBase::onShowXBTInputReady);
   };

   const auto &hdWallet = getCurrentHDWalletFromCombobox(walletsCombobox);
   const auto &leaves = hdWallet->getGroup(hdWallet->getXBTGroupType())->getLeaves();
   std::vector<std::shared_ptr<bs::sync::Wallet>> wallets(leaves.begin(), leaves.end());
   bs::tradeutils::getSpendableTxOutList(wallets, cb);
}

void OTCWindowsAdapterBase::onShowXBTInputReady()
{
   const bool useAutoSel = selectedUTXO_.empty();

   auto inputs = std::make_shared<SelectedTransactionInputs>(allUTXOs_);

   // Set this to false is needed otherwise current selection would be cleared
   inputs->SetUseAutoSel(useAutoSel);

   if (!useAutoSel) {
      for (const auto &utxo : selectedUTXO_) {
         inputs->SetUTXOSelection(utxo.getTxHash(), utxo.getTxOutIndex());
      }
   }

   CoinControlDialog dialog(inputs, true, this);
   int rc = dialog.exec();
   if (rc == QDialog::Accepted) {
      selectedUTXO_ = dialog.selectedInputs();
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
      for (auto wallet : hdWallet->getGroup(hdWallet->getXBTGroupType())->getLeaves()) {
         totalBalance += wallet->getSpendableBalance();
      }
   }
   else {
      for (const auto &utxo : selectedUTXO_) {
         totalBalance += static_cast<double>(utxo.getValue()) / BTCNumericTypes::BalanceDivider;
      }
   }

   return totalBalance;
}

std::shared_ptr<bs::sync::hd::Wallet> OTCWindowsAdapterBase::getCurrentHDWalletFromCombobox(QComboBox *walletsCombobox) const
{
   const auto walletId = walletsCombobox->currentData(UiUtils::WalletIdRole).toString().toStdString();
   return getWalletManager()->getHDWalletById(walletId);
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
