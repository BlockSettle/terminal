#ifndef __OTCWINDOWSMANAGER_H__
#define __OTCWINDOWSMANAGER_H__

#include <memory>
#include "QWidget"
#include "CommonTypes.h"

class OTCWindowsManager;
class AuthAddressManager;
class AssetManager;

namespace bs {
   namespace sync {
      class WalletsManager;
   }
}

class OTCWindowsAdapterBase : public QWidget {
   Q_OBJECT
public:
   OTCWindowsAdapterBase(QWidget* parent = nullptr);
   ~OTCWindowsAdapterBase() override = default;

   void setChatOTCManager(const std::shared_ptr<OTCWindowsManager>& otcManager);
   std::shared_ptr<bs::sync::WalletsManager> getWalletManager() const;
   std::shared_ptr<AuthAddressManager> getAuthManager() const;
   std::shared_ptr<AssetManager> getAssetManager() const;

signals:
   void chatRoomChanged();

protected slots:
   virtual void onSyncInterface();
   virtual void onUpdateMD(bs::network::Asset::Type, const QString&, const bs::network::MDFields&);
   virtual void onUpdateBalances();

protected:
   std::shared_ptr<OTCWindowsManager> otcManager_{};
};

#endif // __OTCWINDOWSMANAGER_H__
