#include "RFQShieldPage.h"
#include "ui_RFQShieldPage.h"
#include "AuthAddressManager.h"


namespace {
   // Label texts
   const QString shieldLoginToSubmitRFQs = QObject::tr("Login to submit RFQs");
   const QString shieldLoginToResponseRFQs = QObject::tr("Login to submit responsive quotes");
   const QString shieldTradingParticipantOnly = QObject::tr("Reserved for Trading Participants");
   const QString shieldDealingParticipantOnly = QObject::tr("Reserved for Dealing Participants");

   const QString shieldCreateCCWallet = QObject::tr("Create %1 wallet");
   const QString shieldCreateAuthLeaf = QObject::tr("Create Authentication wallet");
   const QString shieldGenerateAuthAddress = QObject::tr("Generate an Authentication Address");

   const QString shieldCreateWallet = QObject::tr("To %1 in XBT related products, you require a wallet");
   const QString shieldPromoteToPrimary = QObject::tr("To %1 in XBT related products, you're required to have a wallet which" \
      " can contain the paths required for correctly sorting your tokens and holding" \
      " your Authentication Address(es)");
   const QString shieldTradeUnselectedTargetRequest = QObject::tr("In the Market Data window, please click on the product / security you wish to trade");
   const QString shieldDealingUnselectedTargetRequest = QObject::tr("In the Quote Request Blotter, please click on the product / security you wish to quote");

   // Button texts
   const QString shieldButtonPromote = QObject::tr("Promote");
   const QString shieldButtonCreate = QObject::tr("Create");
   const QString shieldButtonGenerate = QObject::tr("Generate");
}

RFQShieldPage::RFQShieldPage(QWidget *parent) :
   QWidget(parent) ,
   ui_(new Ui::RFQShieldPage())
{
   ui_->setupUi(this);
}

RFQShieldPage::~RFQShieldPage() noexcept = default;

void RFQShieldPage::setShieldButtonAction(std::function<void(void)>&& action)
{
   ui_->shieldButton->disconnect();
   connect(ui_->shieldButton, &QPushButton::clicked, this, [act = std::move(action), this]() {
      ui_->shieldButton->setDisabled(true);
      act();
   });
}

void RFQShieldPage::showShieldLoginToSubmitRequired()
{
   prepareShield(shieldLoginToSubmitRFQs);
}

void RFQShieldPage::showShieldLoginToResponseRequired()
{
   prepareShield(shieldLoginToResponseRFQs);
}

void RFQShieldPage::showShieldReservedTradingParticipant()
{
   prepareShield(shieldTradingParticipantOnly);
}

void RFQShieldPage::showShieldReservedDealingParticipant()
{
   prepareShield(shieldDealingParticipantOnly);
}

void RFQShieldPage::showShieldPromoteToPrimaryWallet()
{
   prepareShield(shieldPromoteToPrimary.arg(tabType_), true, shieldButtonPromote);
}

void RFQShieldPage::showShieldCreateLeaf(const QString& product)
{
   prepareShield(shieldCreateCCWallet.arg(product), true, shieldButtonCreate);
}

void RFQShieldPage::showShieldGenerateAuthAddress()
{
   prepareShield(shieldGenerateAuthAddress, true, shieldButtonGenerate);
}

void RFQShieldPage::setWalletsManager(const std::shared_ptr<bs::sync::WalletsManager> &walletsManager
   , const std::shared_ptr<AuthAddressManager> &authMgr)
{
   walletsManager_ = walletsManager;
   authMgr_ = authMgr;
}

void RFQShieldPage::setTabType(QString&& tabType)
{
   tabType_ = std::move(tabType);
}

bool RFQShieldPage::checkWalletSettings(RFQShieldPage::ProductType productType, const QString& product)
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
         prepareShield(shieldCreateAuthLeaf, true, shieldButtonCreate);
         setShieldButtonAction([this]() {
            walletsManager_->createAuthLeaf(nullptr);
         });
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

RFQShieldPage::ProductType RFQShieldPage::getProductGroup(const QString &productGroup)
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

void RFQShieldPage::showShieldCreateWallet()
{
   prepareShield(shieldCreateWallet.arg(tabType_), true, shieldButtonCreate);
}

void RFQShieldPage::showShieldSelectTargetTrade()
{
   prepareShield(shieldTradeUnselectedTargetRequest);
}

void RFQShieldPage::showShieldSelectTargetDealing()
{
   prepareShield(shieldDealingUnselectedTargetRequest);
}

void RFQShieldPage::prepareShield(const QString& labelText,
   bool showButton /*= false*/, const QString& buttonText /*= QLatin1String()*/)
{
   ui_->shieldText->setText(labelText);

   ui_->shieldButton->setVisible(showButton);
   ui_->shieldButton->setEnabled(showButton);
   ui_->shieldButton->setText(buttonText);
}
