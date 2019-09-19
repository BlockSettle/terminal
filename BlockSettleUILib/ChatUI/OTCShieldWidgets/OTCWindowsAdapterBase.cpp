#include "OTCWindowsAdapterBase.h"
#include "OTCWindowsManager.h"

#include "OtcTypes.h"
#include "UiUtils.h"
#include "Wallets/SyncWalletsManager.h"
#include "Wallets/SyncHDWallet.h"
#include "AuthAddressManager.h"
#include "AssetManager.h"
#include "SelectedTransactionInputs.h"
#include "CoinControlDialog.h"

#include "QComboBox"

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

   connect(otcManager_.get(), &OTCWindowsManager::updateMDDataRequired, this, [this](bs::network::Asset::Type type, const QString& security, const bs::network::MDFields& fields) {
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

void OTCWindowsAdapterBase::onSyncInterface()
{
}

void OTCWindowsAdapterBase::onUpdateMD(bs::network::Asset::Type, const QString&, const bs::network::MDFields&)
{
}

void OTCWindowsAdapterBase::onUpdateBalances()
{
}

void OTCWindowsAdapterBase::showXBTInputsClicked(QComboBox *walletsCombobox)
{
   allUTXOs_.clear();
   awaitingLeafsResponse_.clear();
   selectedUTXO_.clear();

   const auto hdWallet = getCurrentHDWalletFromCombobox(walletsCombobox);
   for (auto wallet : hdWallet->getGroup(hdWallet->getXBTGroupType())->getLeaves()) {
      auto cbUTXOs = [parentWidget = QPointer<OTCWindowsAdapterBase>(this), walletId = wallet->walletId()](const std::vector<UTXO> &utxos) {
         if (!parentWidget) {
            return;
         }

         parentWidget->allUTXOs_.insert(parentWidget->allUTXOs_.end(), utxos.begin(), utxos.end());
         parentWidget->awaitingLeafsResponse_.erase(walletId);

         if (parentWidget->awaitingLeafsResponse_.empty()) {
            QMetaObject::invokeMethod(parentWidget, "onShowXBTInputReady");
         }
      };

      if (!wallet->getSpendableTxOutList(cbUTXOs, UINT64_MAX)) {
         continue;
      }

      awaitingLeafsResponse_.insert(wallet->walletId());
   }
}

void OTCWindowsAdapterBase::onShowXBTInputReady()
{
   auto inputs = std::make_shared<SelectedTransactionInputs>(allUTXOs_);
   CoinControlDialog dialog(inputs, false, this);
   int rc = dialog.exec();
   if (rc == QDialog::Accepted) {
      selectedUTXO_ = dialog.selectedInputs();
   }
   emit xbtInputsProcessed();
}

void OTCWindowsAdapterBase::updateIndicativePrices(bs::network::Asset::Type type, const QString& security, const bs::network::MDFields& fields, double& sellIndicativePrice, double& buyIndicativePrice)
{
   for (const auto &field : fields) {
      switch (field.type) {
      case bs::network::MDField::PriceBid:
         sellIndicativePrice = field.value;
         break;
      case bs::network::MDField::PriceOffer:
         buyIndicativePrice = field.value;
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
      for (auto utxo : selectedUTXO_) {
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

