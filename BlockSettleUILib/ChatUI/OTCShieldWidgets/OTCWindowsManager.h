#ifndef __OTCWINDOWSADAPTERBASE_H__
#define __OTCWINDOWSADAPTERBASE_H__

#include <memory>
#include "QObject"
#include "CommonTypes.h"

namespace bs {
   namespace sync {
      class WalletsManager;
   }
}
class AuthAddressManager;
class MarketDataProvider;
class AssetManager;

namespace bs {
   namespace network {

   }
}

class OTCWindowsManager : public QObject {
   Q_OBJECT
public:
   OTCWindowsManager(QObject* parent = nullptr);
   ~OTCWindowsManager() override = default;

   void init(const std::shared_ptr<bs::sync::WalletsManager>& walletsMgr
      , const std::shared_ptr<AuthAddressManager> &authManager
      , const std::shared_ptr<MarketDataProvider>& mdProvider
      , const std::shared_ptr<AssetManager>& assetManager
   );
   std::shared_ptr<bs::sync::WalletsManager> getWalletManager() const;
   std::shared_ptr<AuthAddressManager> getAuthManager() const;
   std::shared_ptr<AssetManager> getAssetManager() const;

signals:
   void syncInterfaceRequired();
   void updateMDDataRequired(bs::network::Asset::Type, const QString &, const bs::network::MDFields&);
   void updateBalances();

protected:
   std::shared_ptr<bs::sync::WalletsManager> walletsMgr_;
   std::shared_ptr<AuthAddressManager> authManager_;
   std::shared_ptr<MarketDataProvider> mdProvider_;
   std::shared_ptr<AssetManager> assetManager_;
};

#endif // __OTCWINDOWSADAPTERBASE_H__
