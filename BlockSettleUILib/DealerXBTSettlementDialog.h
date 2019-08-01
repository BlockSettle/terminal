#ifndef __DEALER_XBT_SETTLEMENT_DIALOG_H__
#define __DEALER_XBT_SETTLEMENT_DIALOG_H__

#include "BaseDealerSettlementDialog.h"

#include "AuthAddress.h"
#include "CommonTypes.h"
#include "ConnectionManager.h"

#include <string>

// TODO: Obsoleted, delete file after Sign Settlement moved to Signer

namespace Ui {
   class DealerXBTSettlementDialog;
}
namespace bs {
   namespace sync {
      class WalletsManager;
   }
}
class AssetManager;
class DealerXBTSettlementContainer;
class SignContainer;
class BaseCelerClient;
class ApplicationSettings;

class DealerXBTSettlementDialog : public BaseDealerSettlementDialog
{
Q_OBJECT
public:
   DealerXBTSettlementDialog(const std::shared_ptr<spdlog::logger> &
      , const std::shared_ptr<DealerXBTSettlementContainer> &
      , const std::shared_ptr<AssetManager>& assetManager
      , const std::shared_ptr<bs::sync::WalletsManager> &walletsManager
      , const std::shared_ptr<SignContainer> &
      , std::shared_ptr<BaseCelerClient>
      , const std::shared_ptr<ApplicationSettings> &appSettings
      , const std::shared_ptr<ConnectionManager> &
      , QWidget* parent = nullptr);
   ~DealerXBTSettlementDialog() override;

   DealerXBTSettlementDialog(const DealerXBTSettlementDialog&) = delete;
   DealerXBTSettlementDialog& operator = (const DealerXBTSettlementDialog&) = delete;

   DealerXBTSettlementDialog(DealerXBTSettlementDialog&&) = delete;
   DealerXBTSettlementDialog& operator = (DealerXBTSettlementDialog&&) = delete;

protected:
   QWidget *widgetPassword() const override;
   //WalletKeysSubmitWidget *widgetWalletKeys() const override;
   QLabel *labelHint() const override;
   QLabel *labelPassword() const override;

private slots:
   void onAccepted();
   void onActivate();
   void validateGUI() override;

   void onTimerExpired();

   void onRequestorAddressStateChanged(AddressVerificationState);

   void payInDetected(int confirmationsNumber, const BinaryData &txHash);

   void onInfoFromContainer(const QString& text);
   void onErrorFromContainer(const QString& text);

private:
   void deactivate();

   void disableCancelOnOrder();

   void onSettlementFailed();

private:
   std::unique_ptr<Ui::DealerXBTSettlementDialog> ui_;
   std::shared_ptr<DealerXBTSettlementContainer>  settlContainer_;
   bool acceptable_ = false;
};

#endif // __DEALER_XBT_SETTLEMENT_DIALOG_H__
