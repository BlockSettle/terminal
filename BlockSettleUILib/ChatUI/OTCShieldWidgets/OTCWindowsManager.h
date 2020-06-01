/*

***********************************************************************************
* Copyright (C) 2019 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
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
class ArmoryConnection;
class AssetManager;
class AuthAddressManager;
class MDCallbacksQt;

namespace bs {
   class UTXOReservationManager;
}

class OTCWindowsManager : public QObject {
   Q_OBJECT
public:
   OTCWindowsManager(QObject* parent = nullptr);
   ~OTCWindowsManager() override = default;

   void init(const std::shared_ptr<bs::sync::WalletsManager> &
      , const std::shared_ptr<AuthAddressManager> &
      , const std::shared_ptr<MDCallbacksQt> &
      , const std::shared_ptr<AssetManager> &
      , const std::shared_ptr<ArmoryConnection> &
      , const std::shared_ptr<bs::UTXOReservationManager> &);
   std::shared_ptr<bs::sync::WalletsManager> getWalletManager() const;
   std::shared_ptr<AuthAddressManager> getAuthManager() const;
   std::shared_ptr<AssetManager> getAssetManager() const;
   std::shared_ptr<ArmoryConnection> getArmory() const;
   std::shared_ptr<bs::UTXOReservationManager> getUtxoManager() const;

signals:
   void syncInterfaceRequired();
   void updateMDDataRequired(bs::network::Asset::Type, const QString &, const bs::network::MDFields&);
   void updateBalances();

protected:
   std::shared_ptr<bs::sync::WalletsManager> walletsMgr_;
   std::shared_ptr<AuthAddressManager> authManager_;
   std::shared_ptr<AssetManager> assetManager_;
   std::shared_ptr<ArmoryConnection> armory_;
   std::shared_ptr<bs::UTXOReservationManager> utxoReservationManager_;
};

#endif // __OTCWINDOWSADAPTERBASE_H__
