#include "OTCWindowsAdapterBase.h"
#include "OTCWindowsManager.h"

#include "UiUtils.h"
#include "Wallets/SyncWalletsManager.h"
#include "Wallets/SyncHDWallet.h"
#include "AuthAddressManager.h"
#include "AssetManager.h"
#include "SelectedTransactionInputs.h"
#include "CoinControlDialog.h"

#include <QComboBox>
#include <QLabel>

OTCWindowsAdapterBase::OTCWindowsAdapterBase(QWidget* parent /*= nullptr*/)
   : QWidget(parent)
{
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
   auto cb = [handle = validityFlag_.handle(), this](const std::vector<UTXO> &utxos) mutable {
      ValidityGuard guard(handle);
      if (!handle.isValid()) {
         return;
      }
      allUTXOs_ = utxos;
      QMetaObject::invokeMethod(this, &OTCWindowsAdapterBase::onShowXBTInputReady);
   };

   const auto &hdWallet = getCurrentHDWalletFromCombobox(walletsCombobox);
   const auto &leaves = hdWallet->getGroup(hdWallet->getXBTGroupType())->getLeaves();
   std::vector<std::shared_ptr<bs::sync::Wallet>> wallets(leaves.begin(), leaves.end());
   bs::sync::Wallet::getSpendableTxOutList(wallets, cb);
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

void OTCWindowsAdapterBase::setSelectedInputs(const std::vector<UTXO>& selectedUTXO)
{
   selectedUTXO_ = selectedUTXO;
}
