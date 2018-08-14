#ifndef __DEALER_CC_SETTLEMENT_DIALOG_H__
#define __DEALER_CC_SETTLEMENT_DIALOG_H__

#include "BaseDealerSettlementDialog.h"
#include <QString>

namespace Ui {
   class DealerCCSettlementDialog;
}
class DealerCCSettlementContainer;
class WalletsManager;
class CelerClient;


class DealerCCSettlementDialog : public BaseDealerSettlementDialog
{
Q_OBJECT

public:
   DealerCCSettlementDialog(const std::shared_ptr<spdlog::logger> &
      , const std::shared_ptr<DealerCCSettlementContainer> &
      , const std::string &reqRecvAddr
      , std::shared_ptr<WalletsManager> walletsManager
      , const std::shared_ptr<SignContainer> &
      , std::shared_ptr<CelerClient>
      , QWidget* parent = nullptr);
   ~DealerCCSettlementDialog() noexcept override = default;

   DealerCCSettlementDialog(const DealerCCSettlementDialog&) = delete;
   DealerCCSettlementDialog& operator = (const DealerCCSettlementDialog&) = delete;

   DealerCCSettlementDialog(DealerCCSettlementDialog&&) = delete;
   DealerCCSettlementDialog& operator = (DealerCCSettlementDialog&&) = delete;

protected:
   QWidget *widgetPassword() const override;
   WalletKeysSubmitWidget *widgetWalletKeys() const override;
   QLabel *labelHint() const override;
   QLabel *labelPassword() const override;

   void validateGUI() override;

private slots:
   void onAccepted();
   void onGenAddressVerified(bool);

private:
   Ui::DealerCCSettlementDialog*   ui_;
   std::shared_ptr<DealerCCSettlementContainer> settlContainer_;
   const QString                   sValid;
   const QString                   sInvalid;
};

#endif // __DEALER_CC_SETTLEMENT_DIALOG_H__
