
#include "RFQReplyWidget.h"
#include "ui_RFQReplyWidget.h"
#include <spdlog/logger.h>

#include <QDesktopWidget>
#include <QPushButton>

#include "AssetManager.h"
#include "AuthAddressManager.h"
#include "CelerClient.h"
#include "CelerSubmitQuoteNotifSequence.h"
#include "DealerCCSettlementContainer.h"
#include "DealerXBTSettlementContainer.h"
#include "DialogManager.h"
#include "MarketDataProvider.h"
#include "BSMessageBox.h"
#include "OrderListModel.h"
#include "QuoteProvider.h"
#include "RFQDialog.h"
#include "SignContainer.h"
#include "RFQBlotterTreeView.h"
#include "CustomDoubleSpinBox.h"
#include "OrdersView.h"
#include "Wallets/SyncHDWallet.h"
#include "Wallets/SyncWalletsManager.h"

using namespace bs::ui;

enum class DealingPages : int
{
   ShieldPage = 0,
   DealingPage
};

RFQReplyWidget::RFQReplyWidget(QWidget* parent)
   : TabWithShortcut(parent)
   , ui_(new Ui::RFQReplyWidget())
{
   ui_->setupUi(this);
   ui_->shieldPage->setTabType(QLatin1String("dealing"));

   connect(ui_->widgetQuoteRequests, &QuoteRequestsWidget::quoteReqNotifStatusChanged, ui_->pageRFQReply
      , &RFQDealerReply::quoteReqNotifStatusChanged, Qt::QueuedConnection);
   connect(ui_->shieldPage, &RFQShieldPage::requestPrimaryWalletCreation,
      this, &RFQReplyWidget::requestPrimaryWalletCreation);


   ui_->shieldPage->showShieldLoginToResponseRequired();
   popShield();
}

RFQReplyWidget::~RFQReplyWidget() = default;

void RFQReplyWidget::setWalletsManager(const std::shared_ptr<bs::sync::WalletsManager> &walletsManager)
{
   if (!walletsManager_ && walletsManager) {
      walletsManager_ = walletsManager;
      ui_->pageRFQReply->setWalletsManager(walletsManager_);
      ui_->shieldPage->setWalletsManager(walletsManager_, authAddressManager_);

      if (signingContainer_) {
         auto primaryWallet = walletsManager_->getPrimaryWallet();
         if (primaryWallet != nullptr) {
            signingContainer_->GetInfo(primaryWallet->walletId());
         }
      }
   }
}

void RFQReplyWidget::shortcutActivated(ShortcutType s)
{
   switch (s) {
      case ShortcutType::Alt_1 : {
         ui_->widgetQuoteRequests->view()->activate();
      }
         break;

      case ShortcutType::Alt_2 : {
         if (ui_->pageRFQReply->bidSpinBox()->isVisible()) {
            if (ui_->pageRFQReply->bidSpinBox()->isEnabled())
               ui_->pageRFQReply->bidSpinBox()->setFocus();
            else
               ui_->pageRFQReply->offerSpinBox()->setFocus();
         } else {
            ui_->pageRFQReply->setFocus();
         }
      }
         break;

      case ShortcutType::Alt_3 : {
         ui_->treeViewOrders->activate();
      }
         break;

      case ShortcutType::Ctrl_Q : {
         if (ui_->pageRFQReply->quoteButton()->isEnabled())
            ui_->pageRFQReply->quoteButton()->click();
      }
         break;

      case ShortcutType::Ctrl_P : {
         if (ui_->pageRFQReply->pullButton()->isEnabled())
            ui_->pageRFQReply->pullButton()->click();
      }
         break;

      default :
         break;
   }
}

void RFQReplyWidget::init(const std::shared_ptr<spdlog::logger> &logger
   , const std::shared_ptr<BaseCelerClient>& celerClient
   , const std::shared_ptr<AuthAddressManager> &authAddressManager
   , const std::shared_ptr<QuoteProvider>& quoteProvider
   , const std::shared_ptr<MarketDataProvider>& mdProvider
   , const std::shared_ptr<AssetManager>& assetManager
   , const std::shared_ptr<ApplicationSettings> &appSettings
   , const std::shared_ptr<DialogManager> &dialogManager
   , const std::shared_ptr<SignContainer> &container
   , const std::shared_ptr<ArmoryConnection> &armory
   , const std::shared_ptr<ConnectionManager> &connectionManager
   , const std::shared_ptr<bs::DealerUtxoResAdapter> &dealerUtxoAdapter
   , const std::shared_ptr<AutoSignQuoteProvider> &autoSignQuoteProvider)
{
   logger_ = logger;
   celerClient_ = celerClient;
   authAddressManager_ = authAddressManager;
   quoteProvider_ = quoteProvider;
   assetManager_ = assetManager;
   dialogManager_ = dialogManager;
   signingContainer_ = container;
   armory_ = armory;
   appSettings_ = appSettings;
   connectionManager_ = connectionManager;
   dealerUtxoAdapter_ = dealerUtxoAdapter;
   autoSignQuoteProvider_ = autoSignQuoteProvider;

   statsCollector_ = std::make_shared<bs::SecurityStatsCollector>(appSettings, ApplicationSettings::Filter_MD_QN_cnt);
   connect(ui_->pageRFQReply, &RFQDealerReply::submitQuoteNotif, statsCollector_.get(), &bs::SecurityStatsCollector::onQuoteSubmitted);

   ui_->widgetQuoteRequests->init(logger_, quoteProvider_, assetManager, statsCollector_,
                                  appSettings, celerClient_);
   ui_->pageRFQReply->init(logger, authAddressManager, assetManager, quoteProvider_,
                           appSettings, connectionManager, signingContainer_, armory_, dealerUtxoAdapter_, autoSignQuoteProvider_);

   ui_->widgetAutoSignQuote->init(autoSignQuoteProvider_);

   connect(ui_->widgetQuoteRequests, &QuoteRequestsWidget::Selected, this, &RFQReplyWidget::onSelected);

   connect(ui_->pageRFQReply, &RFQDealerReply::submitQuoteNotif, quoteProvider_.get(), &QuoteProvider::SubmitQuoteNotif, Qt::QueuedConnection);
   connect(ui_->pageRFQReply, &RFQDealerReply::submitQuoteNotif, ui_->widgetQuoteRequests, &QuoteRequestsWidget::onQuoteReqNotifReplied);
   connect(ui_->pageRFQReply, &RFQDealerReply::submitQuoteNotif, this, &RFQReplyWidget::onReplied);
   connect(ui_->pageRFQReply, &RFQDealerReply::pullQuoteNotif, quoteProvider_.get(), &QuoteProvider::CancelQuoteNotif);

   connect(mdProvider.get(), &MarketDataProvider::MDUpdate, ui_->widgetQuoteRequests, &QuoteRequestsWidget::onSecurityMDUpdated);
   connect(mdProvider.get(), &MarketDataProvider::MDUpdate, ui_->pageRFQReply, &RFQDealerReply::onMDUpdate);

   connect(quoteProvider_.get(), &QuoteProvider::orderUpdated, this, &RFQReplyWidget::onOrder);
   connect(quoteProvider_.get(), &QuoteProvider::quoteCancelled, ui_->widgetQuoteRequests, &QuoteRequestsWidget::onQuoteReqCancelled);
   connect(quoteProvider_.get(), &QuoteProvider::bestQuotePrice, ui_->widgetQuoteRequests, &QuoteRequestsWidget::onBestQuotePrice, Qt::QueuedConnection);
   connect(quoteProvider_.get(), &QuoteProvider::bestQuotePrice, ui_->pageRFQReply, &RFQDealerReply::onBestQuotePrice, Qt::QueuedConnection);

   connect(quoteProvider_.get(), &QuoteProvider::quoteRejected, ui_->widgetQuoteRequests, &QuoteRequestsWidget::onQuoteRejected);

   connect(quoteProvider_.get(), &QuoteProvider::quoteNotifCancelled, ui_->widgetQuoteRequests, &QuoteRequestsWidget::onQuoteNotifCancelled);
   connect(quoteProvider_.get(), &QuoteProvider::signTxRequested, this, &RFQReplyWidget::onSignTxRequested);

   auto ordersModel = new OrderListModel(quoteProvider_, assetManager, this);
   ui_->treeViewOrders->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
   ui_->treeViewOrders->setModel(ordersModel);
   ui_->treeViewOrders->initWithModel(ordersModel);

   connect(celerClient_.get(), &BaseCelerClient::OnConnectedToServer, this, &RFQReplyWidget::onConnectedToCeler);
   connect(celerClient_.get(), &BaseCelerClient::OnConnectionClosed, this, &RFQReplyWidget::onDisconnectedFromCeler);

   connect(ui_->widgetQuoteRequests->view(), &TreeViewWithEnterKey::enterKeyPressed, this, &RFQReplyWidget::onEnterKeyPressed);
}

void RFQReplyWidget::forceCheckCondition()
{
   const QModelIndex index = ui_->widgetQuoteRequests->view()->selectionModel()->currentIndex();
   if (!index.isValid()) {
      return;
   }
   ui_->widgetQuoteRequests->onQuoteReqNotifSelected(index);
}

void RFQReplyWidget::onReplied(bs::network::QuoteNotification qn)
{
   if (qn.assetType == bs::network::Asset::SpotFX) {
      return;
   }

   const auto &txData = ui_->pageRFQReply->getTransactionData(qn.quoteRequestId);
   if (qn.assetType == bs::network::Asset::SpotXBT) {
      sentXbtTransactionData_[qn.settlementId] = txData;
   } else if (qn.assetType == bs::network::Asset::PrivateMarket) {
      sentCCReplies_[qn.quoteRequestId] = SentCCReply{qn.receiptAddress, txData, qn.reqAuthKey};
   }
}

void RFQReplyWidget::onOrder(const bs::network::Order &order)
{
   if (order.assetType == bs::network::Asset::SpotFX) {
      return;
   }

   if (order.status == bs::network::Order::Pending) {
      if (order.assetType == bs::network::Asset::PrivateMarket) {
         const auto &quoteReqId = quoteProvider_->getQuoteReqId(order.quoteId);
         if (quoteReqId.empty()) {
            logger_->error("[RFQReplyWidget::onOrder] quoteReqId is empty for {}", order.quoteId);
            return;
         }
         const auto itCCSR = sentCCReplies_.find(quoteReqId);
         if (itCCSR == sentCCReplies_.end()) {
            logger_->error("[RFQReplyWidget::onOrder] missing previous CC reply for {}", quoteReqId);
            return;
         }
         const auto &sr = itCCSR->second;
         try {
            const auto settlContainer = std::make_shared<DealerCCSettlementContainer>(logger_, order, quoteReqId
               , assetManager_->getCCLotSize(order.product), assetManager_->getCCGenesisAddr(order.product)
               , sr.recipientAddress, sr.txData->getWallet(), signingContainer_, armory_);
            connect(settlContainer.get(), &DealerCCSettlementContainer::signTxRequest, this, &RFQReplyWidget::saveTxData);
            connect(settlContainer.get(), &bs::SettlementContainer::readyToAccept, this, &RFQReplyWidget::onReadyToAutoSign);

            ui_->widgetQuoteRequests->addSettlementContainer(settlContainer);

//               auto settlDlg = new DealerCCSettlementDialog(logger_, settlContainer,
//                  sr.requestorAuthAddress, walletsManager_, signingContainer_
//                  , celerClient_, appSettings_, connectionManager_, this);
//               showSettlementDialog(settlDlg);
            settlContainer->activate();
         } catch (const std::exception &e) {
            BSMessageBox box(BSMessageBox::critical, tr("Settlement error")
               , tr("Failed to start dealer's CC settlement")
               , QString::fromLatin1(e.what())
               , this);
            box.exec();
         }
      } else {
         auto iTransactionData = sentXbtTransactionData_.find(order.settlementId);
         if (iTransactionData == sentXbtTransactionData_.end()) {
            logger_->debug("[RFQReplyWidget::onOrder] haven't seen QuoteNotif with settlId={}", order.settlementId);
         } else {
            try {
               const auto settlContainer = std::make_shared<DealerXBTSettlementContainer>(logger_, order, walletsManager_
                  , quoteProvider_, iTransactionData->second, authAddressManager_->GetBSAddresses(), signingContainer_
                  , armory_);
               connect(settlContainer.get(), &bs::SettlementContainer::readyToActivate, this, &RFQReplyWidget::onReadyToActivate);
               connect(settlContainer.get(), &bs::SettlementContainer::readyToAccept, this, &RFQReplyWidget::onReadyToAutoSign);

               ui_->widgetQuoteRequests->addSettlementContainer(settlContainer);

//                  auto *dsd = new DealerXBTSettlementDialog(logger_, settlContainer, assetManager_,
//                     walletsManager_, signingContainer_, celerClient_, appSettings_, connectionManager_, this);
//                  showSettlementDialog(dsd);
            } catch (const std::exception &e) {
               logger_->error("[{}] settlement failed: {}", __func__, e.what());
               BSMessageBox box(BSMessageBox::critical, tr("Settlement error")
                  , tr("Failed to start dealer's settlement")
                  , QString::fromLatin1(e.what())
                  , this);
               box.exec();
            }
         }
      }
   } else {
      const auto &quoteReqId = quoteProvider_->getQuoteReqId(order.quoteId);
      if (!quoteReqId.empty()) {
         sentCCReplies_.erase(quoteReqId);
         quoteProvider_->delQuoteReqId(quoteReqId);
      }
      sentXbtTransactionData_.erase(order.settlementId);
   }
}

void RFQReplyWidget::onReadyToAutoSign()
{
   const auto settlContainer = qobject_cast<bs::SettlementContainer *>(sender());
   if (!settlContainer) {
      logger_->error("[RFQReplyWidget::onReadyToAutoSign] failed to cast sender");
      return;
   }
//   if (!settlContainer->accept()) {
//      logger_->warn("[RFQReplyWidget::onReadyToAutoSign] failed to accept");
//      return;
//   }
}

void RFQReplyWidget::onReadyToActivate()
{
   const auto settlContainer = qobject_cast<bs::SettlementContainer *>(sender());
   if (!settlContainer) {
      logger_->error("[{}] failed to cast sender", __func__);
      return;
   }
   settlContainer->activate();
}

void RFQReplyWidget::onConnectedToCeler()
{
   ui_->shieldPage->showShieldSelectTargetDealing();
   popShield();
   ui_->pageRFQReply->onCelerConnected();
}

void RFQReplyWidget::onDisconnectedFromCeler()
{
   ui_->shieldPage->showShieldLoginToResponseRequired();
   popShield();
   ui_->pageRFQReply->onCelerDisconnected();
}

void RFQReplyWidget::onEnterKeyPressed(const QModelIndex &index)
{
   ui_->widgetQuoteRequests->onQuoteReqNotifSelected(index);

   if (ui_->pageRFQReply->quoteButton()->isEnabled()) {
      ui_->pageRFQReply->quoteButton()->click();
      return;
   }

   if (ui_->pageRFQReply->pullButton()->isEnabled()) {
      ui_->pageRFQReply->pullButton()->click();
      return;
   }
}

void RFQReplyWidget::onSelected(const QString& productGroup, const bs::network::QuoteReqNotification& request, double indicBid, double indicAsk)
{
   if (!checkConditions(productGroup, request)) {
      return;
   }

   ui_->pageRFQReply->setQuoteReqNotification(request, indicBid, indicAsk);
}

void RFQReplyWidget::saveTxData(QString orderId, std::string txData)
{
   const auto it = ccTxByOrder_.find(orderId.toStdString());
   if (it != ccTxByOrder_.end()) {
      logger_->debug("[RFQReplyWidget::saveTxData] TX data already requested for order {}", orderId.toStdString());
      quoteProvider_->SignTxRequest(orderId, txData);
      ccTxByOrder_.erase(orderId.toStdString());
   }
   else {
      logger_->debug("[RFQReplyWidget::saveTxData] saving TX data[{}] for order {}", txData.length(), orderId.toStdString());
      ccTxByOrder_[orderId.toStdString()] = txData;
   }
}

void RFQReplyWidget::onSignTxRequested(QString orderId, QString reqId)
{
   Q_UNUSED(reqId);
   const auto it = ccTxByOrder_.find(orderId.toStdString());
   if (it == ccTxByOrder_.end()) {
      logger_->debug("[RFQReplyWidget::onSignTxRequested] no TX data for order {}, yet", orderId.toStdString());
      ccTxByOrder_[orderId.toStdString()] = std::string{};
      return;
   }
   quoteProvider_->SignTxRequest(orderId, it->second);
   ccTxByOrder_.erase(orderId.toStdString());
}


void RFQReplyWidget::showSettlementDialog(QDialog *dlg)
{
   dlg->setAttribute(Qt::WA_DeleteOnClose);

   dialogManager_->adjustDialogPosition(dlg);

   dlg->show();
}

bool RFQReplyWidget::checkConditions(const QString& productGroup , const bs::network::QuoteReqNotification& request)
{
   ui_->stackedWidget->setEnabled(true);

   if (productGroup.isEmpty() || request.product.empty()) {
      ui_->shieldPage->showShieldSelectTargetDealing();
      popShield();
      return true;
   }

   using UserType = CelerClient::CelerUserType;
   const UserType userType = celerClient_->celerUserType();

   using GroupType = RFQShieldPage::ProductType;
   const GroupType group = RFQShieldPage::getProductGroup(productGroup);

   switch (userType) {
   case UserType::Market: {
      if (group == GroupType::SpotFX) {
         ui_->shieldPage->showShieldReservedTradingParticipant();
         popShield();
         return false;
      }
      else if (group == GroupType::SpotXBT) {
         ui_->shieldPage->showShieldReservedDealingParticipant();
         popShield();
         return false;
      }
      else if (ui_->shieldPage->checkWalletSettings(group, QString::fromStdString(request.product))) {
         popShield();
         return false;
      }
      break;
   }
   case UserType::Trading: {
      if (group == GroupType::SpotXBT) {
         ui_->shieldPage->showShieldReservedDealingParticipant();
         return false;
      } else if (group == GroupType::PrivateMarket &&
            ui_->shieldPage->checkWalletSettings(group, QString::fromStdString(request.product))) {
         popShield();
         return false;
      }
      break;
   }
   case UserType::Dealing: {
      if ((group == GroupType::SpotXBT || group == GroupType::PrivateMarket) &&
            ui_->shieldPage->checkWalletSettings(group, QString::fromStdString(request.product))) {
         popShield();
         return false;
      }
      break;
      break;
   }
   default: {
      break;
   }
   }

   if (ui_->stackedWidget->currentIndex() != static_cast<int>(DealingPages::DealingPage)) {
      showEditableRFQPage();
   }

   return true;
}

void RFQReplyWidget::popShield()
{
   ui_->stackedWidget->setEnabled(true);

   ui_->stackedWidget->setCurrentIndex(static_cast<int>(DealingPages::ShieldPage));
   ui_->pageRFQReply->setDisabled(true);
}

void RFQReplyWidget::showEditableRFQPage()
{
   ui_->stackedWidget->setEnabled(true);
   ui_->pageRFQReply->setEnabled(true);
   ui_->stackedWidget->setCurrentIndex(static_cast<int>(DealingPages::DealingPage));
}

