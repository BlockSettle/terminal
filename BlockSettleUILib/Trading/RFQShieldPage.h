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

class RFQShieldPage : public QWidget
{
   Q_OBJECT

public:
   explicit RFQShieldPage(QWidget *parent = nullptr);
   ~RFQShieldPage() noexcept;

   void setShieldButtonAction(std::function<void(void)>&& action);

   void showShieldLoginRequired();
   void showShieldReservedTradingParticipant();
   void showShieldReservedDealingParticipant();
   void showShieldPromoteToPrimaryWallet();
   void showShieldCreateWallet();
   void showShieldSelectTargetTrade();
   void showShieldSelectTargetDealing();
   void showShieldCreateLeaf(const QString& product);

   void setWalletsManager(const std::shared_ptr<bs::sync::WalletsManager>& walletsManager);
   void setTabType(QString&& tabType);

   bool checkWalletSettings(const QString &product);

   using ProductType = bs::network::Asset::Type;
   static ProductType getProductGroup(const QString &productGroup);

signals:
   void requestPrimaryWalletCreation();

protected:
   void prepareShield(const QString& labelText, bool showButton = false,
      const QString& ButtonText = QLatin1String());

private:
   std::unique_ptr<Ui::RFQShieldPage> ui_;
   std::shared_ptr<bs::sync::WalletsManager> walletsManager_;
   QString tabType_;
};

#endif // RFQREPLYLOGINREQUIREDSHIELD_H
