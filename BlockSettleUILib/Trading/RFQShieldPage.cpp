#include "RFQShieldPage.h"
#include "ui_RFQShieldPage.h"

namespace {
   // Label texts
   const QString shieldLogin = QLatin1String("Login to submit RFQs");
   const QString shieldTraidingParticipantOnly = QLatin1String("Reserved for Trading Participants");
   const QString shieldDealingParticipantOnly = QLatin1String("Reserved for Dealing Participants");

   const QString shieldCreateXXXWallet = QLatin1String("Create %1 wallet");
   const QString shieldCreateXBTWallet = QLatin1String("Generate an Authentication Address");

   const QString shieldCreateWallet = QLatin1String("To %1 in XBT related product, you require a wallet");
   const QString shieldPromoteToPrimary = QLatin1String("To %1 in XBT related products, you’re required to have a wallet which" \
      " can contain the paths required for correctly sorting your tokens and holding" \
      " your Authentication Address(es)");
   const QString shieldUnselectedTargetRequest = QLatin1String("In the Market Data window, please click on the product / security you wish to %1");

   // Button texts
   const QString shieldButtonPromote = QLatin1String("Promote");
   const QString shieldButtonCreate = QLatin1String("Create");
   const QString shieldButtonGenerate = QLatin1String("Generate");
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

void RFQShieldPage::showShieldLoginRequiered()
{
   prepareShield(shieldLogin);
}

void RFQShieldPage::showShieldReservedTraidingParticipant()
{
   prepareShield(shieldTraidingParticipantOnly);
}

void RFQShieldPage::showShieldReservedDealingParticipant()
{
   prepareShield(shieldDealingParticipantOnly);
}

void RFQShieldPage::showShieldPromoteToPrimaryWallet()
{
   prepareShield(shieldPromoteToPrimary.arg(tabType_), true, shieldButtonPromote);
}

void RFQShieldPage::showShieldCreateXXXLeaf(const QString& product)
{
   if (product == QLatin1String("XBT")) {
      prepareShield(shieldCreateXBTWallet, true, shieldButtonGenerate);
   } else {
      prepareShield(shieldCreateXXXWallet.arg(product), true, shieldButtonCreate);
   }
}

void RFQShieldPage::setWalletsManager(const std::shared_ptr<bs::sync::WalletsManager>& walletsManager)
{
   walletsManager_ = walletsManager;
}

void RFQShieldPage::setTabType(QString&& tabType)
{
   tabType_ = std::move(tabType);
}

bool RFQShieldPage::checkWalletSettings(const QString &product)
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

   // No path
   if (!walletsManager_->getCCWallet(product.toStdString())) {
      showShieldCreateXXXLeaf(product);
      setShieldButtonAction([this, product]() {
         walletsManager_->CreateCCLeaf(product.toStdString());
      });
      return true;
   }

   return false;
}

RFQShieldPage::ProductGroup RFQShieldPage::getProductGroup(const QString &productGroup)
{
   if (productGroup == QLatin1String("Private Market")) {
      return ProductGroup::PM;
   }
   else if (productGroup == QLatin1String("Spot XBT")) {
      return ProductGroup::XBT;
   }
   else if (productGroup == QLatin1String("Spot FX")) {
      return ProductGroup::FX;
   }
#ifndef QT_NO_DEBUG
   // You need to add logic for new Product group type
   Q_ASSERT(false);
#endif
   return ProductGroup::NONE;
}

void RFQShieldPage::showShieldCreateWallet()
{
   prepareShield(shieldCreateWallet.arg(tabType_), true, shieldButtonCreate);
}

void RFQShieldPage::showShieldSelectTarget()
{
   prepareShield(shieldUnselectedTargetRequest.arg(tabType_));
}

void RFQShieldPage::prepareShield(const QString& labelText,
   bool showButton /*= false*/, const QString& buttonText /*= QLatin1String()*/)
{
   ui_->shieldText->setText(labelText);

   ui_->shieldButton->setVisible(showButton);
   ui_->shieldButton->setEnabled(showButton);
   ui_->shieldButton->setText(buttonText);
}
