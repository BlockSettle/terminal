/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "OTCWindowsManager.h"
#include "Wallets/SyncWalletsManager.h"
#include "AuthAddressManager.h"
#include "MarketDataProvider.h"
#include "AssetManager.h"

OTCWindowsManager::OTCWindowsManager(QObject* parent /*= nullptr*/)
{
}

void OTCWindowsManager::init(const std::shared_ptr<bs::sync::WalletsManager>& walletsMgr
   , const std::shared_ptr<AuthAddressManager> &authManager
   , const std::shared_ptr<MarketDataProvider>& mdProvider
   , const std::shared_ptr<AssetManager>& assetManager
   , const std::shared_ptr<ArmoryConnection> &armory)
{
   // #new_logic : we shouldn't send aggregated signal for all events

   walletsMgr_ = walletsMgr;

   connect(walletsMgr_.get(), &bs::sync::WalletsManager::CCLeafCreated, this, &OTCWindowsManager::syncInterfaceRequired);
   connect(walletsMgr_.get(), &bs::sync::WalletsManager::AuthLeafCreated, this, &OTCWindowsManager::syncInterfaceRequired);
   connect(walletsMgr_.get(), &bs::sync::WalletsManager::walletPromotedToPrimary, this, &OTCWindowsManager::syncInterfaceRequired);
   connect(walletsMgr_.get(), &bs::sync::WalletsManager::walletChanged, this, &OTCWindowsManager::syncInterfaceRequired);
   connect(walletsMgr_.get(), &bs::sync::WalletsManager::walletDeleted, this, &OTCWindowsManager::syncInterfaceRequired);
   connect(walletsMgr_.get(), &bs::sync::WalletsManager::walletAdded, this, &OTCWindowsManager::syncInterfaceRequired);
   connect(walletsMgr_.get(), &bs::sync::WalletsManager::walletsReady, this, &OTCWindowsManager::syncInterfaceRequired);
   connect(walletsMgr_.get(), &bs::sync::WalletsManager::walletsSynchronized, this, &OTCWindowsManager::syncInterfaceRequired);
   connect(walletsMgr_.get(), &bs::sync::WalletsManager::authWalletChanged, this, &OTCWindowsManager::syncInterfaceRequired);

   connect(walletsMgr_.get(), &bs::sync::WalletsManager::walletBalanceUpdated, this, &OTCWindowsManager::updateBalances);

   authManager_ = authManager;

   connect(authManager_.get(), &AuthAddressManager::AddressListUpdated, this, &OTCWindowsManager::syncInterfaceRequired);
   connect(authManager_.get(), &AuthAddressManager::AddrStateChanged, this, &OTCWindowsManager::syncInterfaceRequired);
   connect(authManager_.get(), &AuthAddressManager::AuthWalletChanged, this, &OTCWindowsManager::syncInterfaceRequired);
   connect(authManager_.get(), &AuthAddressManager::AuthWalletCreated, this, &OTCWindowsManager::syncInterfaceRequired);
   connect(authManager_.get(), &AuthAddressManager::ConnectionComplete, this, &OTCWindowsManager::syncInterfaceRequired);
   connect(authManager_.get(), &AuthAddressManager::VerifiedAddressListUpdated, this, &OTCWindowsManager::syncInterfaceRequired);

   mdProvider_ = mdProvider;

   connect(mdProvider_.get(), &MarketDataProvider::MDUpdate, this, &OTCWindowsManager::updateMDDataRequired);

   assetManager_ = assetManager;

   connect(assetManager_.get(), &AssetManager::totalChanged, this, &OTCWindowsManager::updateBalances);
   connect(assetManager_.get(), &AssetManager::securitiesChanged, this, &OTCWindowsManager::updateBalances);

   armory_ = armory;

   emit syncInterfaceRequired();
}

std::shared_ptr<bs::sync::WalletsManager> OTCWindowsManager::getWalletManager() const
{
   return walletsMgr_;
}

std::shared_ptr<AuthAddressManager> OTCWindowsManager::getAuthManager() const
{
   return authManager_;
}

std::shared_ptr<AssetManager> OTCWindowsManager::getAssetManager() const
{
   return assetManager_;
}

std::shared_ptr<ArmoryConnection> OTCWindowsManager::getArmory() const
{
   return armory_;
}

