#include "OTCWindowsManager.h"
#include "Wallets/SyncWalletsManager.h"
#include "AuthAddressManager.h"

OTCWindowsManager::OTCWindowsManager(QObject* parent /*= nullptr*/)
{
}

void OTCWindowsManager::init(const std::shared_ptr<bs::sync::WalletsManager>& walletsMgr, const std::shared_ptr<AuthAddressManager> &authManager)
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

   authManager_ = authManager;

   connect(authManager_.get(), &AuthAddressManager::AddressListUpdated, this, &OTCWindowsManager::syncInterfaceRequired);
   connect(authManager_.get(), &AuthAddressManager::AddrStateChanged, this, &OTCWindowsManager::syncInterfaceRequired);
   connect(authManager_.get(), &AuthAddressManager::AuthWalletChanged, this, &OTCWindowsManager::syncInterfaceRequired);
   connect(authManager_.get(), &AuthAddressManager::AuthWalletCreated, this, &OTCWindowsManager::syncInterfaceRequired);
   connect(authManager_.get(), &AuthAddressManager::ConnectionComplete, this, &OTCWindowsManager::syncInterfaceRequired);
   connect(authManager_.get(), &AuthAddressManager::VerifiedAddressListUpdated, this, &OTCWindowsManager::syncInterfaceRequired);

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

