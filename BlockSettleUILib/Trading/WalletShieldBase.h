#ifndef WALLETSHIELDBASE_H
#define WALLETSHIELDBASE_H

#include <QWidget>
#include <memory>
#include <functional>
#include "CommonTypes.h"

namespace Ui {
   class WalletShieldPage;
}
class AuthAddressManager;
namespace bs {
   namespace sync {
      class WalletsManager;
   }
}

class WalletShieldBase : public QWidget
{
   Q_OBJECT

public:
   explicit WalletShieldBase(QWidget *parent = nullptr);
   virtual ~WalletShieldBase() noexcept;

   void setShieldButtonAction(std::function<void(void)>&& action);

   void init(const std::shared_ptr<bs::sync::WalletsManager> &walletsManager
      , const std::shared_ptr<AuthAddressManager> &authMgr);

   using ProductType = bs::network::Asset::Type;
   bool checkWalletSettings(ProductType productType, const QString &product);
   static ProductType getProductGroup(const QString &productGroup);
   void setTabType(QString&& tabType);

signals:
   void requestPrimaryWalletCreation();

protected:
   void showShield(const QString& labelText,
      const QString& ButtonText = QLatin1String(), const QString& headerText = QLatin1String());


   void showShieldPromoteToPrimaryWallet();
   void showShieldCreateWallet();
   void showShieldCreateLeaf(const QString& product);
   void showShieldGenerateAuthAddress();
   void showShieldAuthValidationProcess();

protected:
   std::unique_ptr<Ui::WalletShieldPage> ui_;
   std::shared_ptr<bs::sync::WalletsManager> walletsManager_;
   std::shared_ptr<AuthAddressManager>       authMgr_;

   QString tabType_;
};

#endif // WALLETSHIELDBASE_H
