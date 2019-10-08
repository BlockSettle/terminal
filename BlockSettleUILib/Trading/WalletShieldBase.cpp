#include "WalletShieldBase.h"

#include <QStackedWidget>
#include "AuthAddressManager.h"
#include "Wallets/SyncWalletsManager.h"

#include "ui_WalletShieldPage.h"


namespace {
   // Label texts
   const QString shieldCreateCCWallet = QObject::tr("Create %1 wallet");
   const QString shieldCreateAuthLeaf = QObject::tr("Create Authentication wallet");
   const QString shieldGenerateAuthAddress = QObject::tr("Generate an Authentication Address");

   const QString shieldCreateWallet = QObject::tr("To %1 in XBT related products, you require a wallet");
   const QString shieldPromoteToPrimary = QObject::tr("To %1 in XBT related products, you're required to have a wallet which" \
      " can contain the paths required for correctly sorting your tokens and holding" \
      " your Authentication Address(es)");

   // Button texts
   const QString shieldButtonPromote = QObject::tr("Promote");
   const QString shieldButtonCreate = QObject::tr("Create");
   const QString shieldButtonGenerate = QObject::tr("Generate");
}

WalletShieldBase::WalletShieldBase(QWidget *parent) :
   QWidget(parent) ,
   ui_(new Ui::WalletShieldPage())
{
   ui_->setupUi(this);
}

WalletShieldBase::~WalletShieldBase() noexcept = default;

void WalletShieldBase::setShieldButtonAction(std::function<void(void)>&& action)
{
   ui_->shieldButton->disconnect();
   connect(ui_->shieldButton, &QPushButton::clicked, this, [act = std::move(action), this]() {
      ui_->shieldButton->setDisabled(true);
      act();
   });
}

void WalletShieldBase::init(const std::shared_ptr<bs::sync::WalletsManager> &walletsManager
   , const std::shared_ptr<AuthAddressManager> &authMgr)
{
   walletsManager_ = walletsManager;
   authMgr_ = authMgr;
}

void WalletShieldBase::setTabType(QString&& tabType)
{
   tabType_ = std::move(tabType);
}

bool WalletShieldBase::checkWalletSettings(WalletShieldBase::ProductType productType, const QString& product)
{
   // No wallet at all
   if (!walletsManager_ || walletsManager_->walletsCount() == 0) {
      showShieldCreateWallet();
      setShieldButtonAction([this]() {
         emit requestPrimaryWalletCreation();
      });
      return true;
   }

   // No primary wallet
   if (!walletsManager_->hasPrimaryWallet()) {
      showShieldPromoteToPrimaryWallet();
      setShieldButtonAction([this]() {
         emit requestPrimaryWalletCreation();
      });
      return true;
   }

   if (productType == ProductType::SpotXBT) {
      if (walletsManager_->getAuthWallet()) {
         if (authMgr_->GetVerifiedAddressList().empty()) {
            showShieldGenerateAuthAddress();
            setShieldButtonAction([this]() {
               emit authMgr_->AuthWalletCreated({});
            });
            return true;
         }
      }
      else {
         showShield(shieldCreateAuthLeaf, true, shieldButtonCreate);
         setShieldButtonAction([this]() {
            walletsManager_->createAuthLeaf(nullptr);
         });
         return true;
      }
   } else if (!walletsManager_->getCCWallet(product.toStdString())) {
      showShieldCreateLeaf(product);
      setShieldButtonAction([this, product]() {
         walletsManager_->CreateCCLeaf(product.toStdString());
      });
      return true;
   }

   return false;
}

WalletShieldBase::ProductType WalletShieldBase::getProductGroup(const QString &productGroup)
{
   if (productGroup == QLatin1String("Private Market")) {
      return ProductType::PrivateMarket;
   }
   else if (productGroup == QLatin1String("Spot XBT")) {
      return ProductType::SpotXBT;
   }
   else if (productGroup == QLatin1String("Spot FX")) {
      return ProductType::SpotFX;
   }
#ifndef QT_NO_DEBUG
   // You need to add logic for new Product group type
   Q_ASSERT(false);
#endif
   return ProductType::Undefined;
}

void WalletShieldBase::showShield(const QString& labelText,
   bool showButton /*= false*/, const QString& buttonText /*= QLatin1String()*/)
{
   ui_->shieldText->setText(labelText);

   ui_->shieldButton->setVisible(showButton);
   ui_->shieldButton->setEnabled(showButton);
   ui_->shieldButton->setText(buttonText);

   QStackedWidget* stack = qobject_cast<QStackedWidget*>(parent());

   // We expected that shield widget will leave only under stack widget
   Q_ASSERT(stack);
   if (!stack) {
      return;
   }

   stack->setCurrentWidget(this);
}

void WalletShieldBase::showShieldPromoteToPrimaryWallet()
{
   showShield(shieldPromoteToPrimary.arg(tabType_), true, shieldButtonPromote);
}

void WalletShieldBase::showShieldCreateWallet()
{
   showShield(shieldCreateWallet.arg(tabType_), true, shieldButtonCreate);
}

void WalletShieldBase::showShieldCreateLeaf(const QString& product)
{
   showShield(shieldCreateCCWallet.arg(product), true, shieldButtonCreate);
}

void WalletShieldBase::showShieldGenerateAuthAddress()
{
   showShield(shieldGenerateAuthAddress, true, shieldButtonGenerate);
}
