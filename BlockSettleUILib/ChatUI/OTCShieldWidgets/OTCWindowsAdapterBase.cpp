#include "OTCWindowsAdapterBase.h"
#include "OTCWindowsManager.h"

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

