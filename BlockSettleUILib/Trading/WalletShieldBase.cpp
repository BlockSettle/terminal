/*

***********************************************************************************
* Copyright (C) 2019 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "WalletShieldBase.h"

#include <QStackedWidget>

#include "AuthAddressManager.h"
#include "Wallets/SyncHDWallet.h"
#include "Wallets/SyncWalletsManager.h"
#include "ApplicationSettings.h"

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

   const QString shieldAuthValidationProcessHeader = QObject::tr("Authentication Address Validation Process");
   const QString shieldAuthValidationProcessText = QObject::tr("Your Authentication Address has been submitted.\n\n"
      "BlockSettle validates the public key against the UserID and executes a transaction from it's Validation Address sometime within a %1 cycle.\n\n"
      "Once executed the Authentication Address need 6 blockchain confirmations to be considered as valid"
   );

   // Button texts
   const QString shieldButtonPromote = QObject::tr("Promote");
   const QString shieldButtonCreate = QObject::tr("Create");
   const QString shieldButtonGenerate = QObject::tr("Generate");
   const QString shieldButtonView = QObject::tr("View");
}

WalletShieldBase::WalletShieldBase(QWidget *parent) :
   QWidget(parent) ,
   ui_(new Ui::WalletShieldPage())
{
   ui_->setupUi(this);
}

WalletShieldBase::~WalletShieldBase() noexcept = default;

void WalletShieldBase::setShieldButtonAction(std::function<void(void)>&& action, bool oneShot)
{
   ui_->shieldButton->disconnect();
   connect(ui_->shieldButton, &QPushButton::clicked, this, [act = std::move(action), this, oneShot]() {
      if (oneShot) {
         ui_->shieldButton->setDisabled(true);
      }
      act();
   });
}

void WalletShieldBase::init(const std::shared_ptr<bs::sync::WalletsManager> &walletsManager,
   const std::shared_ptr<AuthAddressManager> &authMgr, const std::shared_ptr<ApplicationSettings> &appSettings)
{
   walletsManager_ = walletsManager;
   authMgr_ = authMgr;
   appSettings_ = appSettings;
}

void WalletShieldBase::setTabType(QString&& tabType)
{
   tabType_ = std::move(tabType);
}

bool WalletShieldBase::checkWalletSettings(WalletShieldBase::ProductType productType, const QString& product)
{
   // No primary wallet
   bool hasFullWallet = false;
   if (walletsManager_) {
      for (const auto &wallet : walletsManager_->hdWallets()) {
         if (wallet->isFullWallet()) {
            hasFullWallet = true;
            break;
         }
      }
   }

   if (!walletsManager_ || (!walletsManager_->hasPrimaryWallet() && !hasFullWallet)) {
      showShieldCreateWallet();
      setShieldButtonAction([this]() {
         emit requestPrimaryWalletCreation();
      });
      return true;
   }

   if (!walletsManager_->hasPrimaryWallet()) {
      assert(hasFullWallet);
      showShieldPromoteToPrimaryWallet();
      setShieldButtonAction([this]() {
         emit requestPrimaryWalletCreation();
      });
      return true;
   }

   if (productType == ProductType::SpotXBT) {
      if (walletsManager_->getAuthWallet()) {
         const bool isNoVerifiedAddresses = authMgr_->GetSubmittedAddressList().empty();
         if (isNoVerifiedAddresses && authMgr_->isAtLeastOneAwaitingVerification())
         {
            showShieldAuthValidationProcess();
            setShieldButtonAction([this]() {
               emit authMgr_->AuthWalletCreated({});
            });
            return true;
         }
         else if (isNoVerifiedAddresses) {
            showShieldGenerateAuthAddress();
            setShieldButtonAction([this]() {
               emit authMgr_->AuthWalletCreated({});
            });
            return true;
         }
      }
      else {
         showShield(shieldCreateAuthLeaf, shieldButtonCreate);
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
   else if (productGroup == QLatin1String("1day Deliverable")) {
      return ProductType::Futures;
   }
#ifndef QT_NO_DEBUG
   // You need to add logic for new Product group type
   Q_ASSERT(false);
#endif
   return ProductType::Undefined;
}

void WalletShieldBase::showShield(const QString& labelText,
  const QString& buttonText /*= QLatin1String()*/, const QString& headerText /*= QLatin1String()*/)
{
   ui_->shieldText->setText(labelText);

   const bool isShowButton = !buttonText.isEmpty();
   ui_->shieldButton->setVisible(isShowButton);
   ui_->shieldButton->setEnabled(isShowButton);
   ui_->shieldButton->setText(buttonText);

   ui_->shieldHeaderText->setVisible(!headerText.isEmpty());
   ui_->shieldHeaderText->setText(headerText);

   ui_->secondInfoBlock->hide();
   ui_->thirdInfoBlock->hide();

   raiseInStack();
}

void WalletShieldBase::showTwoBlockShield(const QString& headerText1, const QString& labelText1,
   const QString& headerText2, const QString& labelText2)
{
   ui_->shieldButton->setVisible(false);

   ui_->shieldHeaderText->setText(headerText1);
   ui_->shieldText->setText(labelText1);
   ui_->shieldHeaderText->setVisible(true);

   ui_->shieldHeaderTextSecond->setText(headerText2);
   ui_->shieldTextSecond->setText(labelText2);
   ui_->secondInfoBlock->setVisible(true);

   ui_->thirdInfoBlock->hide();
   raiseInStack();
}

void WalletShieldBase::showThreeBlockShield(const QString& headerText1, const QString& labelText1,
   const QString& headerText2, const QString& labelText2,
   const QString& headerText3, const QString& labelText3)
{
   ui_->shieldButton->setVisible(false);

   ui_->shieldHeaderText->setText(headerText1);
   ui_->shieldText->setText(labelText1);
   ui_->shieldHeaderText->setVisible(true);

   ui_->shieldHeaderTextSecond->setText(headerText2);
   ui_->shieldTextSecond->setText(labelText2);
   ui_->secondInfoBlock->setVisible(true);

   ui_->shieldHeaderTextThird->setText(headerText3);
   ui_->shieldTextThird->setText(labelText3);
   ui_->thirdInfoBlock->setVisible(true);

   raiseInStack();
}

void WalletShieldBase::raiseInStack()
{
   QStackedWidget* stack = qobject_cast<QStackedWidget*>(parent());

   // We expected that shield widget will leave only under stack widget
   assert(stack);
   if (!stack) {
      return;
   }

   stack->setCurrentWidget(this);
}

void WalletShieldBase::showShieldPromoteToPrimaryWallet()
{
   showShield(shieldPromoteToPrimary.arg(tabType_), shieldButtonPromote);
}

void WalletShieldBase::showShieldCreateWallet()
{
   showShield(shieldCreateWallet.arg(tabType_), shieldButtonCreate);
}

void WalletShieldBase::showShieldCreateLeaf(const QString& product)
{
   showShield(shieldCreateCCWallet.arg(product), shieldButtonCreate);
}

void WalletShieldBase::showShieldGenerateAuthAddress()
{
   showShield(shieldGenerateAuthAddress, shieldButtonGenerate);
}

void WalletShieldBase::showShieldAuthValidationProcess()
{
   const bool isProd = appSettings_->get<int>(ApplicationSettings::envConfiguration) ==
      static_cast<int>(ApplicationSettings::EnvConfiguration::Production);

   showShield(shieldAuthValidationProcessText.arg(isProd ? tr("24h") : tr("15 minutes")),
      shieldButtonView, shieldAuthValidationProcessHeader);
}
