#include "RFQRequestWidget.h"

#include <QLineEdit>
#include <QPushButton>

#include "ApplicationSettings.h"
#include "AuthAddressManager.h"
#include "CelerClient.h"
#include "DialogManager.h"
#include "NotificationCenter.h"
#include "OrderListModel.h"
#include "OrdersView.h"
#include "QuoteProvider.h"
#include "RFQDialog.h"
#include "SignContainer.h"
#include "ui_RFQRequestWidget.h"


namespace  {
   const QString shieldLogin = QLatin1String("Login to submit RFQs");
   const QString shieldTraidingParticipantOnly = QLatin1String("Reserved for Trading Participants");

   enum class RFQPages : int
   {
      ShieldPage = 0,
      EditableRFQPage
   };

   enum class ProductGroup
   {
      PM = 0,
      XBT,
      FX,
      NONE
   };

   ProductGroup getProductGroup(const QString &productGroup)
   {
      if (productGroup == QLatin1String("Private Market")) {
         return ProductGroup::PM;
      } else if (productGroup == QLatin1String("Spot XBT")) {
         return ProductGroup::XBT;
      } else if (productGroup == QLatin1String("Spot FX")) {
         return ProductGroup::FX;
      }
#ifndef QT_NO_DEBUG
      // You need to add logic for new Product group type
      Q_ASSERT(false);
#endif
      return ProductGroup::NONE;
   }
}

RFQRequestWidget::RFQRequestWidget(QWidget* parent)
   : TabWithShortcut(parent)
   , ui_(new Ui::RFQRequestWidget())
{
   ui_->setupUi(this);

   connect(ui_->pageRFQTicket, &RFQTicketXBT::submitRFQ, this, &RFQRequestWidget::onRFQSubmit);
}

RFQRequestWidget::~RFQRequestWidget() = default;

void RFQRequestWidget::setWalletsManager(const std::shared_ptr<bs::sync::WalletsManager> &walletsManager)
{
   if (walletsManager_ == nullptr) {
      walletsManager_ = walletsManager;
      ui_->pageRFQTicket->setWalletsManager(walletsManager);
   }
}

void RFQRequestWidget::shortcutActivated(ShortcutType s)
{
   switch (s) {
      case ShortcutType::Alt_1 : {
         ui_->widgetMarketData->view()->activate();
      }
         break;

      case ShortcutType::Alt_2 : {
         if (ui_->pageRFQTicket->lineEditAmount()->isVisible())
            ui_->pageRFQTicket->lineEditAmount()->setFocus();
         else
            ui_->pageRFQTicket->setFocus();
      }
         break;

      case ShortcutType::Alt_3 : {
         ui_->treeViewOrders->activate();
      }
         break;

      case ShortcutType::Ctrl_S : {
         if (ui_->pageRFQTicket->submitButton()->isEnabled())
            ui_->pageRFQTicket->submitButton()->click();
      }
         break;

      case ShortcutType::Alt_S : {
         ui_->pageRFQTicket->sellButton()->click();
      }
         break;

      case ShortcutType::Alt_B : {
         ui_->pageRFQTicket->buyButton()->click();
      }
         break;

      case ShortcutType::Alt_P : {
         if (ui_->pageRFQTicket->numCcyButton()->isChecked())
            ui_->pageRFQTicket->denomCcyButton()->click();
         else
            ui_->pageRFQTicket->numCcyButton()->click();
      }
         break;

      default :
         break;
   }
}

void RFQRequestWidget::setAuthorized(bool authorized)
{
   ui_->widgetMarketData->setAuthorized(authorized);
}

void RFQRequestWidget::showShieldLoginRequiered()
{
   prepareAndPopShield(shieldLogin);
}

void RFQRequestWidget::showShieldReservedTraidingParticipant()
{
   prepareAndPopShield(shieldTraidingParticipantOnly);
}

void RFQRequestWidget::showEditableRFQPage()
{
   ui_->stackedWidgetRFQ->setCurrentIndex(static_cast<int>(RFQPages::EditableRFQPage));
   ui_->pageRFQTicket->enablePanel();
}

void RFQRequestWidget::prepareAndPopShield(const QString& labelText)
{
   ui_->shieldButton->hide();
   ui_->shieldText->setText(labelText);
   ui_->stackedWidgetRFQ->setCurrentIndex(static_cast<int>(RFQPages::ShieldPage));
   ui_->pageRFQTicket->disablePanel();
}

void RFQRequestWidget::initWidgets(const std::shared_ptr<MarketDataProvider>& mdProvider
   , const std::shared_ptr<ApplicationSettings> &appSettings)
{
   appSettings_ = appSettings;
   ui_->widgetMarketData->init(appSettings, ApplicationSettings::Filter_MD_RFQ, mdProvider);
}

void RFQRequestWidget::init(std::shared_ptr<spdlog::logger> logger
   , const std::shared_ptr<BaseCelerClient>& celerClient
   , const std::shared_ptr<AuthAddressManager> &authAddressManager
   , std::shared_ptr<QuoteProvider> quoteProvider
   , const std::shared_ptr<AssetManager>& assetManager
   , const std::shared_ptr<DialogManager> &dialogManager
   , const std::shared_ptr<SignContainer> &container
   , const std::shared_ptr<ArmoryConnection> &armory
   , const std::shared_ptr<ConnectionManager> &connectionManager)
{
   logger_ = logger;
   celerClient_ = celerClient;
   authAddressManager_ = authAddressManager;
   quoteProvider_ = quoteProvider;
   assetManager_ = assetManager;
   dialogManager_ = dialogManager;
   signingContainer_ = container;
   armory_ = armory;
   connectionManager_ = connectionManager;

   ui_->pageRFQTicket->init(authAddressManager, assetManager, quoteProvider, container, armory);

   auto ordersModel = new OrderListModel(quoteProvider_, assetManager, ui_->treeViewOrders);
   ui_->treeViewOrders->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
   ui_->treeViewOrders->setModel(ordersModel);
   ui_->treeViewOrders->initWithModel(ordersModel);
   connect(quoteProvider_.get(), &QuoteProvider::quoteOrderFilled, this, [](const std::string &quoteId) {
      NotificationCenter::notify(bs::ui::NotifyType::CelerOrder, {true, QString::fromStdString(quoteId)});
   });
   connect(quoteProvider_.get(), &QuoteProvider::orderFailed, this, [](const std::string &quoteId, const std::string &reason) {
      NotificationCenter::notify(bs::ui::NotifyType::CelerOrder
         , { false, QString::fromStdString(quoteId), QString::fromStdString(reason) });
   });

   connect(celerClient_.get(), &BaseCelerClient::OnConnectedToServer, this, &RFQRequestWidget::onConnectedToCeler);
   connect(celerClient_.get(), &BaseCelerClient::OnConnectionClosed, this, &RFQRequestWidget::onDisconnectedFromCeler);

   ui_->pageRFQTicket->disablePanel();
}

void RFQRequestWidget::onConnectedToCeler()
{
   connect(ui_->widgetMarketData, &MarketDataWidget::CurrencySelected, this, &RFQRequestWidget::onCurrencySelected);
   connect(ui_->widgetMarketData, &MarketDataWidget::BuyClicked, this, &RFQRequestWidget::onBuyClicked);
   connect(ui_->widgetMarketData, &MarketDataWidget::SellClicked, this, &RFQRequestWidget::onSellClicked);

   showEditableRFQPage();
}

void RFQRequestWidget::onDisconnectedFromCeler()
{  
   disconnect(ui_->widgetMarketData, &MarketDataWidget::CurrencySelected, this, &RFQRequestWidget::onCurrencySelected);
   disconnect(ui_->widgetMarketData, &MarketDataWidget::BuyClicked, this, &RFQRequestWidget::onBuyClicked);
   disconnect(ui_->widgetMarketData, &MarketDataWidget::SellClicked, this, &RFQRequestWidget::onSellClicked);

   showShieldLoginRequiered();
}

void RFQRequestWidget::onRFQSubmit(const bs::network::RFQ& rfq)
{
   auto authAddr = ui_->pageRFQTicket->selectedAuthAddress();

   RFQDialog* dialog = new RFQDialog(logger_, rfq, ui_->pageRFQTicket->GetTransactionData(), quoteProvider_,
      authAddressManager_, assetManager_, walletsManager_, signingContainer_, armory_, celerClient_, appSettings_, connectionManager_, authAddr, this);

   dialog->setAttribute(Qt::WA_DeleteOnClose);

   dialogManager_->adjustDialogPosition(dialog);
   dialog->show();

   ui_->pageRFQTicket->resetTicket();
}

bool RFQRequestWidget::checkConditions(const QString &productGroup)
{
   using UserType = CelerClient::CelerUserType;
   const UserType userType = celerClient_->celerUserType();

   const ProductGroup group = getProductGroup(productGroup);

   switch (userType) {
   case UserType::Market: {
      if (group == ProductGroup::XBT || group == ProductGroup::FX) {
         showShieldReservedTraidingParticipant();
         return false;
      } if (ui_->stackedWidgetRFQ->currentIndex() != static_cast<int>(RFQPages::EditableRFQPage)) {
         showEditableRFQPage();
      }
      break;
   }
   case UserType::Dealing: {
      break;
   }
   case UserType::Trading: {
      break;
   }
   default: {
      break;
   }
   }

   return true;
}

void RFQRequestWidget::onCurrencySelected(const QString &productGroup, const QString &currencyPair,
                                          const QString &bidPrice, const QString &offerPrice)
{
   if (!checkConditions(productGroup))
      return;

   ui_->pageRFQTicket->setSecurityId(productGroup, currencyPair,
                                     bidPrice, offerPrice);
}

void RFQRequestWidget::onBuyClicked(const QString &productGroup, const QString &currencyPair,
                                    const QString &bidPrice, const QString &offerPrice)
{
   if (!checkConditions(productGroup))
      return;

   ui_->pageRFQTicket->setSecuritySell(productGroup, currencyPair,
                                     bidPrice, offerPrice);
}

void RFQRequestWidget::onSellClicked(const QString &productGroup, const QString &currencyPair,
                                     const QString &bidPrice, const QString &offerPrice)
{
   if (!checkConditions(productGroup))
      return;

   ui_->pageRFQTicket->setSecurityBuy(productGroup, currencyPair,
                                     bidPrice, offerPrice);
}
