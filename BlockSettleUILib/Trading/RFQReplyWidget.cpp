/*

***********************************************************************************
* Copyright (C) 2016 - , BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "RFQReplyWidget.h"
#include "ui_RFQReplyWidget.h"

#include "AssetManager.h"
#include "AuthAddressManager.h"
#include "BSMessageBox.h"
#include "CelerClient.h"
#include "CelerSubmitQuoteNotifSequence.h"
#include "CustomControls/CustomDoubleSpinBox.h"
#include "DealerCCSettlementContainer.h"
#include "DealerXBTSettlementContainer.h"
#include "DialogManager.h"
#include "MDCallbacksQt.h"
#include "OrderListModel.h"
#include "OrdersView.h"
#include "QuoteProvider.h"
#include "RFQBlotterTreeView.h"
#include "SelectedTransactionInputs.h"
#include "SignContainer.h"
#include "Wallets/SyncHDWallet.h"
#include "Wallets/SyncWalletsManager.h"
#include "UtxoReservationManager.h"

#include "bs_proxy_terminal_pb.pb.h"

#include <spdlog/logger.h>

#include <QDesktopWidget>
#include <QPushButton>

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
      ui_->shieldPage->init(walletsManager_, authAddressManager_);

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
   , const std::shared_ptr<MDCallbacksQt>& mdCallbacks
   , const std::shared_ptr<AssetManager>& assetManager
   , const std::shared_ptr<ApplicationSettings> &appSettings
   , const std::shared_ptr<DialogManager> &dialogManager
   , const std::shared_ptr<SignContainer> &container
   , const std::shared_ptr<ArmoryConnection> &armory
   , const std::shared_ptr<ConnectionManager> &connectionManager
   , const std::shared_ptr<AutoSignQuoteProvider> &autoSignQuoteProvider
   , const std::shared_ptr<bs::UTXOReservationManager> &utxoReservationManager
   , OrderListModel *orderListModel
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
   appSettings_ = appSettings;
   connectionManager_ = connectionManager;
   autoSignQuoteProvider_ = autoSignQuoteProvider;
   utxoReservationManager_ = utxoReservationManager;

   statsCollector_ = std::make_shared<bs::SecurityStatsCollector>(appSettings, ApplicationSettings::Filter_MD_QN_cnt);

   ui_->widgetQuoteRequests->init(logger_, quoteProvider_, assetManager, statsCollector_,
                                  appSettings, celerClient_);
   ui_->pageRFQReply->init(logger, authAddressManager, assetManager, quoteProvider_,
                           appSettings, connectionManager, signingContainer_, armory_, autoSignQuoteProvider_, utxoReservationManager);

   ui_->widgetAutoSignQuote->init(autoSignQuoteProvider_);

   connect(ui_->widgetQuoteRequests, &QuoteRequestsWidget::Selected, this, &RFQReplyWidget::onSelected);

   ui_->pageRFQReply->setSubmitQuoteNotifCb([this](const std::shared_ptr<bs::ui::SubmitQuoteReplyData> &data) {
      statsCollector_->onQuoteSubmitted(data->qn);
      quoteProvider_->SubmitQuoteNotif(data->qn);
      ui_->widgetQuoteRequests->onQuoteReqNotifReplied(data->qn);
      onReplied(data);
   });

   ui_->pageRFQReply->setGetLastSettlementReply([this](const std::string& settlementId) -> const std::vector<UTXO>* {
      auto lastReply = sentXbtReplies_.find(settlementId);
      if (lastReply == sentXbtReplies_.end()) {
         return nullptr;
      }

      return &(lastReply->second.utxosPayinFixed);
   });

   ui_->pageRFQReply->setResetCurrentReservation([this](const std::shared_ptr<bs::ui::SubmitQuoteReplyData> &data) {
      onResetCurrentReservation(data);
   });

   connect(ui_->pageRFQReply, &RFQDealerReply::pullQuoteNotif, this, &RFQReplyWidget::onPulled);

   connect(mdCallbacks.get(), &MDCallbacksQt::MDUpdate, ui_->widgetQuoteRequests, &QuoteRequestsWidget::onSecurityMDUpdated);
   connect(mdCallbacks.get(), &MDCallbacksQt::MDUpdate, ui_->pageRFQReply, &RFQDealerReply::onMDUpdate);

   connect(quoteProvider_.get(), &QuoteProvider::orderUpdated, this, &RFQReplyWidget::onOrder);
   connect(quoteProvider_.get(), &QuoteProvider::quoteCancelled, this, &RFQReplyWidget::onQuoteCancelled);
   connect(quoteProvider_.get(), &QuoteProvider::bestQuotePrice, ui_->widgetQuoteRequests, &QuoteRequestsWidget::onBestQuotePrice, Qt::QueuedConnection);
   connect(quoteProvider_.get(), &QuoteProvider::bestQuotePrice, ui_->pageRFQReply, &RFQDealerReply::onBestQuotePrice, Qt::QueuedConnection);

   connect(quoteProvider_.get(), &QuoteProvider::quoteRejected, this, &RFQReplyWidget::onQuoteRejected);

   connect(quoteProvider_.get(), &QuoteProvider::quoteNotifCancelled, this, &RFQReplyWidget::onQuoteNotifCancelled);
   connect(quoteProvider_.get(), &QuoteProvider::allQuoteNotifCancelled, ui_->widgetQuoteRequests, &QuoteRequestsWidget::onAllQuoteNotifCancelled);
   connect(quoteProvider_.get(), &QuoteProvider::signTxRequested, this, &RFQReplyWidget::onSignTxRequested);

   ui_->treeViewOrders->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
   ui_->treeViewOrders->setModel(orderListModel);
   ui_->treeViewOrders->initWithModel(orderListModel);


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

void RFQReplyWidget::onReplied(const std::shared_ptr<bs::ui::SubmitQuoteReplyData> &data)
{
   switch (data->qn.assetType) {
      case bs::network::Asset::SpotXBT: {
         assert(data->xbtWallet);
         if (sentReplyIdsToSettlementsIds_.count(data->qn.quoteRequestId)) {
            break; // already answered, nothing to do there
         }
         sentReplyIdsToSettlementsIds_[data->qn.quoteRequestId] = data->qn.settlementId;
         auto &reply = sentXbtReplies_[data->qn.settlementId];
         reply.xbtWallet = data->xbtWallet;
         reply.authAddr = data->authAddr;
         reply.utxosPayinFixed = data->fixedXbtInputs;
         reply.utxoRes = std::move(data->utxoRes);
         break;
      }

      case bs::network::Asset::PrivateMarket: {
         assert(data->xbtWallet);
         auto &reply = sentCCReplies_[data->qn.quoteRequestId];
         reply.recipientAddress = data->qn.receiptAddress;
         reply.requestorAuthAddress = data->qn.reqAuthKey;
         reply.utxoRes = std::move(data->utxoRes);
         reply.xbtWallet = data->xbtWallet;
         break;
      }

      default: {
         break;
      }
   }
}

void RFQReplyWidget::onPulled(const std::string& settlementId, const std::string& reqId, const std::string& reqSessToken)
{
   sentXbtReplies_.erase(settlementId);
   sentReplyIdsToSettlementsIds_.erase(reqId);
   quoteProvider_->CancelQuoteNotif(QString::fromStdString(reqId), QString::fromStdString(reqSessToken));
}

void RFQReplyWidget::onResetCurrentReservation(const std::shared_ptr<SubmitQuoteReplyData> &data)
{
   switch (data->qn.assetType) {
      case bs::network::Asset::PrivateMarket: {
         auto it = sentCCReplies_.find(data->qn.quoteRequestId);
         if (it != sentCCReplies_.end()) {
            it->second.utxoRes.release();
         }
         break;
      }

      default: {
         break;
      }
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
            SPDLOG_LOGGER_ERROR(logger_, "quoteReqId is empty for {}", order.quoteId);
            return;
         }
         const auto itCCSR = sentCCReplies_.find(quoteReqId);
         if (itCCSR == sentCCReplies_.end()) {
            SPDLOG_LOGGER_DEBUG(logger_, "missing previous CC reply for {}", quoteReqId);
            return;
         }
         auto &sr = itCCSR->second;
         try {
            const auto settlContainer = std::make_shared<DealerCCSettlementContainer>(logger_, order, quoteReqId
               , assetManager_->getCCLotSize(order.product), assetManager_->getCCGenesisAddr(order.product)
               , sr.recipientAddress, sr.xbtWallet, signingContainer_, armory_, walletsManager_, std::move(sr.utxoRes));
            connect(settlContainer.get(), &DealerCCSettlementContainer::signTxRequest, this, &RFQReplyWidget::saveTxData);
            connect(settlContainer.get(), &DealerCCSettlementContainer::error, this, &RFQReplyWidget::onTransactionError);
            connect(settlContainer.get(), &DealerCCSettlementContainer::cancelTrade, this, &RFQReplyWidget::cancelCCTrade);

            connect(quoteProvider_.get(), &QuoteProvider::orderFailed, this
                    , [settlContainer, quoteId = order.quoteId](const std::string& failedQuoteId, const std::string& reason){
               if (quoteId == failedQuoteId) {
                  settlContainer->cancel();
               }
            });

            ui_->widgetQuoteRequests->addSettlementContainer(settlContainer);
            settlContainer->activate();

         } catch (const std::exception &e) {
            BSMessageBox box(BSMessageBox::critical, tr("Settlement error")
               , tr("Failed to start dealer's CC settlement")
               , QString::fromLatin1(e.what())
               , this);
            box.exec();
         }
      } else {
         auto it = sentXbtReplies_.find(order.settlementId);
         if (it == sentXbtReplies_.end()) {
            // Looks like this is not error, not sure why we need this
            SPDLOG_LOGGER_DEBUG(logger_, "haven't seen QuoteNotif with settlId={}", order.settlementId);
            return;
         }
         try {
            auto &reply = it->second;
            // Dealers can't select receiving address, use new
            const auto recvXbtAddr = bs::Address();
            const auto settlContainer = std::make_shared<DealerXBTSettlementContainer>(logger_, order
               , walletsManager_, reply.xbtWallet, quoteProvider_, signingContainer_, armory_, authAddressManager_
               , reply.authAddr, reply.utxosPayinFixed, recvXbtAddr, utxoReservationManager_, std::move(reply.utxoRes));

            connect(settlContainer.get(), &DealerXBTSettlementContainer::sendUnsignedPayinToPB, this, &RFQReplyWidget::sendUnsignedPayinToPB);
            connect(settlContainer.get(), &DealerXBTSettlementContainer::sendSignedPayinToPB, this, &RFQReplyWidget::sendSignedPayinToPB);
            connect(settlContainer.get(), &DealerXBTSettlementContainer::sendSignedPayoutToPB, this, &RFQReplyWidget::sendSignedPayoutToPB);
            connect(settlContainer.get(), &DealerXBTSettlementContainer::cancelTrade, this, &RFQReplyWidget::cancelXBTTrade);

            connect(settlContainer.get(), &DealerXBTSettlementContainer::error, this, &RFQReplyWidget::onTransactionError);

            connect(this, &RFQReplyWidget::unsignedPayinRequested, settlContainer.get(), &DealerXBTSettlementContainer::onUnsignedPayinRequested);
            connect(this, &RFQReplyWidget::signedPayoutRequested, settlContainer.get(), &DealerXBTSettlementContainer::onSignedPayoutRequested);
            connect(this, &RFQReplyWidget::signedPayinRequested, settlContainer.get(), &DealerXBTSettlementContainer::onSignedPayinRequested);

            connect(quoteProvider_.get(), &QuoteProvider::orderFailed, this
                    , [settlContainer, quoteId = order.quoteId](const std::string& failedQuoteId, const std::string& reason){
               if (quoteId == failedQuoteId) {
                  settlContainer->cancel();
               }
            });

            // Add before calling activate as this will hook some events
            ui_->widgetQuoteRequests->addSettlementContainer(settlContainer);

            settlContainer->activate();

         } catch (const std::exception &e) {
            SPDLOG_LOGGER_ERROR(logger_, "settlement failed: {}", e.what());
            BSMessageBox box(BSMessageBox::critical, tr("Settlement error")
               , tr("Failed to start dealer's settlement")
               , QString::fromLatin1(e.what())
               , this);
            box.exec();
         }
      }
   } else {
      const auto &quoteReqId = quoteProvider_->getQuoteReqId(order.quoteId);
      if (!quoteReqId.empty()) {
         sentCCReplies_.erase(quoteReqId);
         quoteProvider_->delQuoteReqId(quoteReqId);
      }
      sentXbtReplies_.erase(order.settlementId);
   }
}

void RFQReplyWidget::onQuoteCancelled(const QString &reqId, bool userCancelled)
{
   eraseReply(reqId);
   ui_->widgetQuoteRequests->onQuoteReqCancelled(reqId, userCancelled);
}

void RFQReplyWidget::onQuoteRejected(const QString &reqId, const QString &reason)
{
   eraseReply(reqId);
   ui_->widgetQuoteRequests->onQuoteRejected(reqId, reason);
}

void RFQReplyWidget::onQuoteNotifCancelled(const QString &reqId)
{
   eraseReply(reqId);
   ui_->widgetQuoteRequests->onQuoteNotifCancelled(reqId);
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

void RFQReplyWidget::onTransactionError(bs::error::ErrorCode code, const QString& error)
{
   if (bs::error::ErrorCode::TxCanceled != code) {
      MessageBoxBroadcastError(error, this).exec();
   }
}

void RFQReplyWidget::saveTxData(QString orderId, std::string txData)
{
   quoteProvider_->SignTxRequest(orderId, txData);
}

void RFQReplyWidget::onSignTxRequested(QString orderId, QString reqId, QDateTime timestamp)
{
   Q_UNUSED(reqId);

   if (!ui_->widgetQuoteRequests->StartCCSignOnOrder(orderId, timestamp)) {
      logger_->error("[RFQReplyWidget::onSignTxRequested] failed to initiate sign on CC order: {}"
                     , orderId.toStdString());
   }
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


void RFQReplyWidget::eraseReply(const QString &reqId)
{
   auto settlement = sentReplyIdsToSettlementsIds_.find(reqId.toStdString());
   if (settlement != sentReplyIdsToSettlementsIds_.end()) {
      sentXbtReplies_.erase(settlement->second);
      sentReplyIdsToSettlementsIds_.erase(settlement);
   }
}

void RFQReplyWidget::hideEvent(QHideEvent* event)
{
   ui_->pageRFQReply->onParentAboutToHide();
   QWidget::hideEvent(event);
}

void RFQReplyWidget::onMessageFromPB(const Blocksettle::Communication::ProxyTerminalPb::Response &response)
{
   switch (response.data_case()) {
      case Blocksettle::Communication::ProxyTerminalPb::Response::kSendUnsignedPayin: {
         const auto &command = response.send_unsigned_payin();
         emit unsignedPayinRequested(command.settlement_id());
         break;
      }

      case Blocksettle::Communication::ProxyTerminalPb::Response::kSignPayout: {
         const auto &command = response.sign_payout();
         auto timestamp = QDateTime::fromMSecsSinceEpoch(command.timestamp_ms());
         // payin_data - payin hash . binary
         emit signedPayoutRequested(command.settlement_id(), BinaryData::fromString(command.payin_data()), timestamp);
         break;
      }

      case Blocksettle::Communication::ProxyTerminalPb::Response::kSignPayin: {
         auto command = response.sign_payin();
         auto timestamp = QDateTime::fromMSecsSinceEpoch(command.timestamp_ms());
         // unsigned_payin_data - serialized payin. binary
         emit signedPayinRequested(command.settlement_id(), BinaryData::fromString(command.unsigned_payin_data()), timestamp);
         break;
      }

      default:
         break;
   }

   // if not processed - not RFQ releated message. not error
}
