#ifndef __OTCWINDOWSMANAGER_H__
#define __OTCWINDOWSMANAGER_H__

#include <memory>
#include "QWidget"
#include "CommonTypes.h"

class QComboBox;
class OTCWindowsManager;
class AuthAddressManager;
class AssetManager;

namespace bs {
   namespace sync {
      class WalletsManager;

      namespace hd {
         class Wallet;
      }
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
   void xbtInputsProcessed();

protected slots:
   virtual void onSyncInterface();
   virtual void onUpdateMD(bs::network::Asset::Type, const QString&, const bs::network::MDFields&);
   virtual void onUpdateBalances();
   void onShowXBTInputReady();
protected:

   // Shared function between children
   void showXBTInputsClicked(QComboBox *walletsCombobox);
   
   void updateIndicativePrices(
      bs::network::Asset::Type type
      , const QString& security
      , const bs::network::MDFields& fields
      , double& sellIndicativePrice
      , double& buyIndicativePrice);
   
   BTCNumericTypes::balance_type getXBTSpendableBalanceFromCombobox(QComboBox *walletsCombobox) const;
   std::shared_ptr<bs::sync::hd::Wallet> getCurrentHDWalletFromCombobox(QComboBox *walletsCombobox) const;

protected:
   std::shared_ptr<OTCWindowsManager> otcManager_{};

   std::set<std::string> awaitingLeafsResponse_;
   std::vector<UTXO> allUTXOs_;
   std::vector<UTXO> selectedUTXO_;
};

#endif // __OTCWINDOWSMANAGER_H__
