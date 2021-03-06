/*

***********************************************************************************
* Copyright (C) 2019 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
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
class ApplicationSettings;

class WalletShieldBase : public QWidget
{
   Q_OBJECT

public:
   explicit WalletShieldBase(QWidget *parent = nullptr);
   virtual ~WalletShieldBase() noexcept;

   void setShieldButtonAction(std::function<void(void)>&& action, bool oneShot = true);

   void init(const std::shared_ptr<bs::sync::WalletsManager> &walletsManager
      , const std::shared_ptr<AuthAddressManager> &authMgr
      , const std::shared_ptr<ApplicationSettings> &appSettings);

   using ProductType = bs::network::Asset::Type;
   bool checkWalletSettings(ProductType productType, const QString &product);
   static ProductType getProductGroup(const QString &productGroup);
   void setTabType(QString&& tabType);

signals:
   void requestPrimaryWalletCreation();
   void loginRequested();

protected:
   void showShield(const QString& labelText,
      const QString& ButtonText = QLatin1String(), const QString& headerText = QLatin1String());

   void showTwoBlockShield(const QString& headerText1, const QString& labelText1,
      const QString& headerText2, const QString& labelText2);

   void showThreeBlockShield(const QString& headerText1, const QString& labelText1,
      const QString& headerText2, const QString& labelText2,
      const QString& headerText3, const QString& labelText3);

   void raiseInStack();

   void showShieldPromoteToPrimaryWallet();
   void showShieldCreateWallet();
   void showShieldCreateLeaf(const QString& product);
   void showShieldGenerateAuthAddress();
   void showShieldAuthValidationProcess();

protected:
   std::unique_ptr<Ui::WalletShieldPage> ui_;
   std::shared_ptr<bs::sync::WalletsManager> walletsManager_;
   std::shared_ptr<AuthAddressManager>       authMgr_;
   std::shared_ptr<ApplicationSettings>      appSettings_;

   QString tabType_;
};

#endif // WALLETSHIELDBASE_H
