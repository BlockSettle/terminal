#ifndef __OTCWINDOWSMANAGER_H__
#define __OTCWINDOWSMANAGER_H__

#include <memory>
#include "CommonTypes.h"
#include "OtcTypes.h"
#include "QWidget"
#include "ValidityFlag.h"

class QComboBox;
class OTCWindowsManager;
class AuthAddressManager;
class AssetManager;
class QLabel;

namespace bs {
   namespace sync {
      class WalletsManager;

      namespace hd {
         class Wallet;
      }
   }

   namespace network {
      namespace otc {
         struct Peer;
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

   virtual void setPeer(const bs::network::otc::Peer &);
signals:
   
   void xbtInputsProcessed();

public slots:
   virtual void onAboutToApply();
   virtual void onChatRoomChanged();

protected slots:
   virtual void onSyncInterface();
   void onUpdateMD(bs::network::Asset::Type, const QString&, const bs::network::MDFields&);
   virtual void onMDUpdated();
   virtual void onUpdateBalances();
   void onShowXBTInputReady();

protected:

   // Shared function between children
   void showXBTInputsClicked(QComboBox *walletsCombobox);
   
   void updateIndicativePrices(
      bs::network::Asset::Type type
      , const QString& security
      , const bs::network::MDFields& fields);
   
   BTCNumericTypes::balance_type getXBTSpendableBalanceFromCombobox(QComboBox *walletsCombobox) const;
   std::shared_ptr<bs::sync::hd::Wallet> getCurrentHDWalletFromCombobox(QComboBox *walletsCombobox) const;

   QString getXBTRange(bs::network::otc::Range xbtRange);
   QString getCCRange(bs::network::otc::Range ccRange);

   QString getSide(bs::network::otc::Side requestSide, bool isOwnRequest);

   const std::vector<UTXO> &selectedUTXOs() const { return selectedUTXO_; }
   void clearSelectedInputs();
   void setSelectedInputs(const std::vector<UTXO>& selectedUTXO);

protected:
   std::shared_ptr<OTCWindowsManager> otcManager_{};

   bs::network::Asset::Type productGroup_ = bs::network::Asset::SpotXBT;
   // #new_logic : fix security & product checking
   QString security_{ QLatin1String("XBT/EUR") };
   QString sellProduct_{ QLatin1String("XBT") };
   QString buyProduct_{ QLatin1String("EUR") };
   double sellIndicativePrice_{};
   double buyIndicativePrice_{};

   ValidityFlag validityFlag_;

private:
   std::vector<UTXO> allUTXOs_;
   std::vector<UTXO> selectedUTXO_;

};

#endif // __OTCWINDOWSMANAGER_H__
