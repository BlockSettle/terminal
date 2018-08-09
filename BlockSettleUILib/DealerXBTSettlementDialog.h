#ifndef __DEALER_XBT_SETTLEMENT_DIALOG_H__
#define __DEALER_XBT_SETTLEMENT_DIALOG_H__

#include "BaseDealerSettlementDialog.h"

#include "AuthAddress.h"
#include "CommonTypes.h"
#include "SettlementWallet.h"

#include <string>

namespace Ui {
   class DealerXBTSettlementDialog;
}
namespace bs {
   class SettlementAddressEntry;
}
class AssetManager;
class DealerXBTSettlementContainer;
class SignContainer;
class WalletsManager;
class CelerClient;


class DealerXBTSettlementDialog : public BaseDealerSettlementDialog
{
Q_OBJECT
public:
   DealerXBTSettlementDialog(const std::shared_ptr<spdlog::logger> &
      , const std::shared_ptr<DealerXBTSettlementContainer> &
      , const std::shared_ptr<AssetManager>& assetManager
      , std::shared_ptr<WalletsManager> walletsManager
      , const std::shared_ptr<SignContainer> &
      , std::shared_ptr<CelerClient>
      , QWidget* parent = nullptr);
   ~DealerXBTSettlementDialog() noexcept override = default;

   DealerXBTSettlementDialog(const DealerXBTSettlementDialog&) = delete;
   DealerXBTSettlementDialog& operator = (const DealerXBTSettlementDialog&) = delete;

   DealerXBTSettlementDialog(DealerXBTSettlementDialog&&) = delete;
   DealerXBTSettlementDialog& operator = (DealerXBTSettlementDialog&&) = delete;

protected:
   QWidget *widgetPassword() const override;
   QLineEdit *lineEditPassword() const override;
   QLabel *labelHint() const override;
   QLabel *labelPassword() const override;

private slots:
   void onAccepted();
   void updateControls();

   void onTimerExpired();

   void onRequestorAddressStateChanged(AddressVerificationState);

   void payInDetected(int confirmationsNumber, const BinaryData &txHash);

   void onInfoFromContainer(const QString& text);
   void onErrorFromContainer(const QString& text);

private:
   void activate();
   void deactivate();

   void disableCancelOnOrder();

   void onSettlementFailed();

private:
   Ui::DealerXBTSettlementDialog*   ui_;
   std::shared_ptr<DealerXBTSettlementContainer>   settlContainer_;
   bool acceptable_ = false;
};

#endif // __DEALER_XBT_SETTLEMENT_DIALOG_H__
