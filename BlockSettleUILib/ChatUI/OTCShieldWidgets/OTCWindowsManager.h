#ifndef __OTCWINDOWSADAPTERBASE_H__
#define __OTCWINDOWSADAPTERBASE_H__

#include <memory>
#include "QObject"

namespace bs {
   namespace sync {
      class WalletsManager;
   }
}
class AuthAddressManager;

class OTCWindowsManager : public QObject {
   Q_OBJECT
public:
   OTCWindowsManager(QObject* parent = nullptr);
   ~OTCWindowsManager() override = default;

   void init(const std::shared_ptr<bs::sync::WalletsManager>& walletsMgr, const std::shared_ptr<AuthAddressManager> &authManager);
   std::shared_ptr<bs::sync::WalletsManager> getWalletManager() const;
   std::shared_ptr<AuthAddressManager> getAuthManager() const;
signals:
   void syncInterfaceRequired();

protected:
   std::shared_ptr<bs::sync::WalletsManager> walletsMgr_;
   std::shared_ptr<AuthAddressManager> authManager_;
};

#endif // __OTCWINDOWSADAPTERBASE_H__
