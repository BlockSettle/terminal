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
#include "Wallets/SyncWalletsManager.h"
#include "CurrencyPair.h"
#include "ui_RFQRequestWidget.h"


namespace  {
   enum class RFQPages : int
   {
      ShieldPage = 0,
      EditableRFQPage
   };
}

RFQRequestWidget::RFQRequestWidget(QWidget* parent)
   : TabWithShortcut(parent)
   , ui_(new Ui::RFQRequestWidget())
{
   ui_->setupUi(this);
   ui_->shieldPage->setTabType(QLatin1String("trade"));

   connect(ui_->pageRFQTicket, &RFQTicketXBT::submitRFQ, this, &RFQRequestWidget::onRFQSubmit);
   connect(ui_->shieldPage, &RFQShieldPage::requestPrimaryWalletCreation, this, &RFQRequestWidget::requestPrimaryWalletCreation);
   
   ui_->shieldPage->showShieldLoginToSubmitRequired();
   popShield();
}

RFQRequestWidget::~RFQRequestWidget() = default;

void RFQRequestWidget::setWalletsManager(const std::shared_ptr<bs::sync::WalletsManager> &walletsManager)
{
   if (walletsManager_ == nullptr) {
      walletsManager_ = walletsManager;
      ui_->pageRFQTicket->setWalletsManager(walletsManager);
      ui_->shieldPage->setWalletsManager(walletsManager, authAddressManager_);

      connect(walletsManager_.get(), &bs::sync::WalletsManager::CCLeafCreated, this, &RFQRequestWidget::forceCheckCondition);
      connect(walletsManager_.get(), &bs::sync::WalletsManager::AuthLeafCreated, this, &RFQRequestWidget::forceCheckCondition);
      connect(walletsManager_.get(), &bs::sync::WalletsManager::walletChanged, this, &RFQRequestWidget::forceCheckCondition);
      connect(walletsManager_.get(), &bs::sync::WalletsManager::walletDeleted, this, &RFQRequestWidget::forceCheckCondition);
      connect(walletsManager_.get(), &bs::sync::WalletsManager::walletAdded, this, &RFQRequestWidget::forceCheckCondition);
      connect(walletsManager_.get(), &bs::sync::WalletsManager::walletsReady, this, &RFQRequestWidget::forceCheckCondition);
      connect(walletsManager_.get(), &bs::sync::WalletsManager::walletsSynchronized, this, &RFQRequestWidget::forceCheckCondition);
      connect(walletsManager_.get(), &bs::sync::WalletsManager::walletPromotedToPrimary, this, &RFQRequestWidget::forceCheckCondition);
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
         if (ui_->pageRFQTicket->lineEditAmount()->isVisible()) {
            ui_->pageRFQTicket->lineEditAmount()->setFocus();
         }
         else {
            ui_->pageRFQTicket->setFocus();
         }
      }
         break;

      case ShortcutType::Alt_3 : {
         ui_->treeViewOrders->activate();
      }
         break;

      case ShortcutType::Ctrl_S : {
         if (ui_->pageRFQTicket->submitButton()->isEnabled()) {
            ui_->pageRFQTicket->submitButton()->click();
         }
      }
         break;

      case ShortcutType::Alt_S : {
         if (ui_->pageRFQTicket->isEnabled()) {
            ui_->pageRFQTicket->sellButton()->click();
         }
      }
         break;

      case ShortcutType::Alt_B : {
         if (ui_->pageRFQTicket->isEnabled()) {
            ui_->pageRFQTicket->buyButton()->click();
         }
      }
         break;

      case ShortcutType::Alt_P : {
         if (ui_->pageRFQTicket->isEnabled()) {
            if (ui_->pageRFQTicket->numCcyButton()->isChecked()) {
               ui_->pageRFQTicket->denomCcyButton()->click();
            }
            else {
               ui_->pageRFQTicket->numCcyButton()->click();
            }
         }
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

void RFQRequestWidget::showEditableRFQPage()
{
   ui_->stackedWidgetRFQ->setEnabled(true);
   ui_->pageRFQTicket->enablePanel();
   ui_->stackedWidgetRFQ->setCurrentIndex(static_cast<int>(RFQPages::EditableRFQPage));
}

void RFQRequestWidget::popShield()
{
   ui_->stackedWidgetRFQ->setEnabled(true);

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
   , const std::shared_ptr<ConnectionManager> &connectionManager
)
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
   connect(quoteProvider_.get(), &QuoteProvider::quoteOrderFilled, [](const std::string &quoteId) {
      NotificationCenter::notify(bs::ui::NotifyType::CelerOrder, {true, QString::fromStdString(quoteId)});
   });
   connect(quoteProvider_.get(), &QuoteProvider::orderFailed, [](const std::string &quoteId, const std::string &reason) {
      NotificationCenter::notify(bs::ui::NotifyType::CelerOrder
         , { false, QString::fromStdString(quoteId), QString::fromStdString(reason) });
   });

   connect(celerClient_.get(), &BaseCelerClient::OnConnectedToServer, this, &RFQRequestWidget::onConnectedToCeler);
   connect(celerClient_.get(), &BaseCelerClient::OnConnectionClosed, this, &RFQRequestWidget::onDisconnectedFromCeler);

   ui_->pageRFQTicket->disablePanel();
}

void RFQRequestWidget::onConnectedToCeler()
{
   marketDataConnection.push_back(connect(ui_->widgetMarketData, &MarketDataWidget::CurrencySelected,
                                          this, &RFQRequestWidget::onCurrencySelected));
   marketDataConnection.push_back(connect(ui_->widgetMarketData, &MarketDataWidget::BidClicked,
                                          this, &RFQRequestWidget::onBidClicked));
   marketDataConnection.push_back(connect(ui_->widgetMarketData, &MarketDataWidget::AskClicked,
                                          this, &RFQRequestWidget::onAskClicked));
   marketDataConnection.push_back(connect(ui_->widgetMarketData, &MarketDataWidget::MDHeaderClicked,
                                          this, &RFQRequestWidget::onDisableSelectedInfo));

   ui_->shieldPage->showShieldSelectTargetTrade();
   popShield();
}

void RFQRequestWidget::onDisconnectedFromCeler()
{  
   for (QMetaObject::Connection &conn : marketDataConnection) {
      QObject::disconnect(conn);
   }

   ui_->shieldPage->showShieldLoginToSubmitRequired();
   popShield();
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

   const auto& currentInfo = ui_->widgetMarketData->getCurrentlySelectedInfo();
   ui_->pageRFQTicket->SetProductAndSide(currentInfo.productGroup_, currentInfo.currencyPair_,
                                         currentInfo.bidPrice_, currentInfo.offerPrice_, bs::network::Side::Undefined);
}

bool RFQRequestWidget::checkConditions(const MarketSelectedInfo& selectedInfo)
{
   ui_->stackedWidgetRFQ->setEnabled(true);
   using UserType = CelerClient::CelerUserType;
   const UserType userType = celerClient_->celerUserType();

   using GroupType = RFQShieldPage::ProductType;
   const GroupType group = RFQShieldPage::getProductGroup(selectedInfo.productGroup_);

   switch (userType) {
   case UserType::Market: {
      if (group == GroupType::SpotFX || group == GroupType::SpotXBT) {
         ui_->shieldPage->showShieldReservedTradingParticipant();
         popShield();
         return false;
      } else if (checkWalletSettings(group, selectedInfo)) {
         return false;
      }
      break;
   }
   case UserType::Dealing:
   case UserType::Trading: {
      if ((group == GroupType::SpotXBT || group == GroupType::PrivateMarket) &&
         checkWalletSettings(group, selectedInfo)) {
         return false;
      }
      break;
   }
   default: {
      break;
   }
   }

   if (ui_->stackedWidgetRFQ->currentIndex() != static_cast<int>(RFQPages::EditableRFQPage)) {
      showEditableRFQPage();
   }

   return true;
}

bool RFQRequestWidget::checkWalletSettings(bs::network::Asset::Type productType, const MarketSelectedInfo& selectedInfo)
{
   const CurrencyPair cp(selectedInfo.currencyPair_.toStdString());
   const QString currentProduct = QString::fromStdString(cp.NumCurrency());
   if (ui_->shieldPage->checkWalletSettings(productType, currentProduct)) {
      popShield();
      return true;
   }

   return false;
}

void RFQRequestWidget::forceCheckCondition()
{
   if (!ui_->widgetMarketData) {
      return;
   }

   const auto& currentInfo = ui_->widgetMarketData->getCurrentlySelectedInfo();

   if (!currentInfo.isValid()) {
      return;
   }
}

void RFQRequestWidget::onCurrencySelected(const MarketSelectedInfo& selectedInfo)
{
   if (!checkConditions(selectedInfo)) {
      return;
   }

   ui_->pageRFQTicket->setSecurityId(selectedInfo.productGroup_, selectedInfo.currencyPair_,
                                     selectedInfo.bidPrice_, selectedInfo.offerPrice_);
}

void RFQRequestWidget::onBidClicked(const MarketSelectedInfo& selectedInfo)
{
   if (!checkConditions(selectedInfo)) {
      return;
   }

   ui_->pageRFQTicket->setSecuritySell(selectedInfo.productGroup_, selectedInfo.currencyPair_,
                                       selectedInfo.bidPrice_, selectedInfo.offerPrice_);
}

void RFQRequestWidget::onAskClicked(const MarketSelectedInfo& selectedInfo)
{
   if (!checkConditions(selectedInfo)) {
      return;
   }

   ui_->pageRFQTicket->setSecurityBuy(selectedInfo.productGroup_, selectedInfo.currencyPair_,
                                       selectedInfo.bidPrice_, selectedInfo.offerPrice_);
}

void RFQRequestWidget::onDisableSelectedInfo()
{
   ui_->shieldPage->showShieldSelectTargetTrade();
   popShield();
}
