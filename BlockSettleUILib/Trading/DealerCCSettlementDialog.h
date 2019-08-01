#ifndef __DEALER_CC_SETTLEMENT_DIALOG_H__
#define __DEALER_CC_SETTLEMENT_DIALOG_H__

#include "BaseDealerSettlementDialog.h"
#include <QString>

// TODO: Obsoleted, delete file after Sign Settlement moved to Signer

namespace Ui {
   class DealerCCSettlementDialog;
}
namespace bs {
   namespace sync {
      class WalletsManager;
   }
}
class BaseCelerClient;
class DealerCCSettlementContainer;


class DealerCCSettlementDialog : public BaseDealerSettlementDialog
{
Q_OBJECT

public:
   DealerCCSettlementDialog(const std::shared_ptr<spdlog::logger> &
      , const std::shared_ptr<DealerCCSettlementContainer> &
      , const std::string &reqRecvAddr
      , const std::shared_ptr<bs::sync::WalletsManager> &walletsManager
      , const std::shared_ptr<SignContainer> &
      , std::shared_ptr<BaseCelerClient>
      , const std::shared_ptr<ApplicationSettings> &appSettings
      , const std::shared_ptr<ConnectionManager> &connectionManager
      , QWidget* parent = nullptr);
   ~DealerCCSettlementDialog() override;

   DealerCCSettlementDialog(const DealerCCSettlementDialog&) = delete;
   DealerCCSettlementDialog& operator = (const DealerCCSettlementDialog&) = delete;

   DealerCCSettlementDialog(DealerCCSettlementDialog&&) = delete;
   DealerCCSettlementDialog& operator = (DealerCCSettlementDialog&&) = delete;

protected:
   QWidget *widgetPassword() const override;
   //WalletKeysSubmitWidget *widgetWalletKeys() const override;
   QLabel *labelHint() const override;
   QLabel *labelPassword() const override;

   void validateGUI() override;

private slots:
   void onAccepted();
   void onGenAddressVerified(bool);

private:
   std::unique_ptr<Ui::DealerCCSettlementDialog> ui_;
   std::shared_ptr<DealerCCSettlementContainer> settlContainer_;
   const QString                   sValid;
   const QString                   sInvalid;
};

#endif // __DEALER_CC_SETTLEMENT_DIALOG_H__
