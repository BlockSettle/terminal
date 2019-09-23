#ifndef RFQREPLYLOGINREQUIREDSHIELD_H
#define RFQREPLYLOGINREQUIREDSHIELD_H

#include <QWidget>
#include <memory>
#include <functional>
#include "CommonTypes.h"
#include "Wallets/SyncWalletsManager.h"

namespace Ui {
   class RFQShieldPage;
}
class AuthAddressManager;

class RFQShieldPage : public QWidget
{
   Q_OBJECT

public:
   explicit RFQShieldPage(QWidget *parent = nullptr);
   ~RFQShieldPage() noexcept;

   void setShieldButtonAction(std::function<void(void)>&& action);

   void showShieldLoginToSubmitRequired();
   void showShieldLoginToResponseRequired();
   void showShieldReservedTradingParticipant();
   void showShieldReservedDealingParticipant();
   void showShieldPromoteToPrimaryWallet();
   void showShieldCreateWallet();
   void showShieldSelectTargetTrade();
   void showShieldSelectTargetDealing();
   void showShieldCreateLeaf(const QString& product);
   void showShieldGenerateAuthAddress();

   void setWalletsManager(const std::shared_ptr<bs::sync::WalletsManager> &
      , const std::shared_ptr<AuthAddressManager> &);
   void setTabType(QString&& tabType);

   using ProductType = bs::network::Asset::Type;
   bool checkWalletSettings(ProductType productType, const QString &product);
   static ProductType getProductGroup(const QString &productGroup);

signals:
   void requestPrimaryWalletCreation();

protected:
   void prepareShield(const QString& labelText, bool showButton = false,
      const QString& ButtonText = QLatin1String());

private:
   std::unique_ptr<Ui::RFQShieldPage> ui_;
   std::shared_ptr<bs::sync::WalletsManager> walletsManager_;
   std::shared_ptr<AuthAddressManager>       authMgr_;
   QString tabType_;
};

#endif // RFQREPLYLOGINREQUIREDSHIELD_H
