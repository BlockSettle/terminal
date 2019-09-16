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
      syncInterface();
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

void OTCWindowsAdapterBase::syncInterface()
{
}

