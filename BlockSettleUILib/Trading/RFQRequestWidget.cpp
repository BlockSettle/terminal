/*

***********************************************************************************
* Copyright (C) 2019 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "RFQRequestWidget.h"

#include <QLineEdit>
#include <QPushButton>

#include "ApplicationSettings.h"
#include "AuthAddressManager.h"
#include "AutoSignQuoteProvider.h"
#include "BSMessageBox.h"
#include "Celer/CelerClient.h"
#include "CurrencyPair.h"
#include "DialogManager.h"
#include "HeadlessContainer.h"
#include "MDCallbacksQt.h"
#include "NotificationCenter.h"
#include "OrderListModel.h"
#include "OrdersView.h"
#include "QuoteProvider.h"
#include "RFQDialog.h"
#include "RfqStorage.h"
#include "UserScriptRunner.h"
#include "UtxoReservationManager.h"
#include "Wallets/SyncHDWallet.h"
#include "Wallets/SyncWalletsManager.h"

#include "bs_proxy_terminal_pb.pb.h"

#include "ui_RFQRequestWidget.h"

using namespace Blocksettle::Communication;
namespace  {
   enum class RFQPages : int
   {
      ShieldPage = 0,
      EditableRFQPage,
      Futures,
   };
}

RFQRequestWidget::RFQRequestWidget(QWidget* parent)
   : TabWithShortcut(parent)
   , ui_(new Ui::RFQRequestWidget())
{
   rfqStorage_ = std::make_shared<RfqStorage>();

   ui_->setupUi(this);
   ui_->shieldPage->setTabType(QLatin1String("trade"));

   connect(ui_->shieldPage, &RFQShieldPage::requestPrimaryWalletCreation, this, &RFQRequestWidget::requestPrimaryWalletCreation);
   connect(ui_->shieldPage, &RFQShieldPage::loginRequested, this, &RFQRequestWidget::loginRequested);

   connect(ui_->pageRFQTicket, &RFQTicketXBT::needWalletData, this, &RFQRequestWidget::needWalletData);
   connect(ui_->pageRFQTicket, &RFQTicketXBT::needAuthKey, this, &RFQRequestWidget::needAuthKey);
   connect(ui_->pageRFQTicket, &RFQTicketXBT::needReserveUTXOs, this, &RFQRequestWidget::needReserveUTXOs);

   ui_->pageRFQTicket->setSubmitRFQ([this]
      (const std::string &id, const bs::network::RFQ& rfq, bs::UtxoReservationToken utxoRes)
   {
      onRFQSubmit(id, rfq, std::move(utxoRes));
   });
   ui_->pageRFQTicket->setCancelRFQ([this] (const std::string &id)
   {
      onRFQCancel(id);
   });

   ui_->shieldPage->showShieldLoginToSubmitRequired();

   ui_->pageRFQTicket->lineEditAmount()->installEventFilter(this);
   popShield();
}

RFQRequestWidget::~RFQRequestWidget() = default;

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

void RFQRequestWidget::onNewSecurity(const std::string& name, bs::network::Asset::Type at)
{
   ui_->pageRFQTicket->onNewSecurity(name, at);
}

void RFQRequestWidget::onMDUpdated(bs::network::Asset::Type assetType
   , const QString& security, const bs::network::MDFields &fields)
{
   ui_->widgetMarketData->onMDUpdated(assetType, security, fields);
}

void RFQRequestWidget::onBalance(const std::string& currency, double balance)
{
   ui_->pageRFQTicket->onBalance(currency, balance);
   balances_[currency] = balance;
}

void RFQRequestWidget::onWalletBalance(const bs::sync::WalletBalanceData& wbd)
{
   ui_->pageRFQTicket->onWalletBalance(wbd);
}

void RFQRequestWidget::onHDWallet(const bs::sync::HDWalletData& wallet)
{
   ui_->pageRFQTicket->onHDWallet(wallet);
}

void RFQRequestWidget::onWalletData(const std::string& walletId
   , const bs::sync::WalletData& wd)
{
   ui_->pageRFQTicket->onWalletData(walletId, wd);
}

void RFQRequestWidget::onMatchingLogin(const std::string& mtchLogin
   , BaseCelerClient::CelerUserType userType, const std::string& userId)
{
   marketDataConnection_.push_back(connect(ui_->widgetMarketData, &MarketDataWidget::CurrencySelected,
      this, &RFQRequestWidget::onCurrencySelected));
   marketDataConnection_.push_back(connect(ui_->widgetMarketData, &MarketDataWidget::BidClicked,
      this, &RFQRequestWidget::onBidClicked));
   marketDataConnection_.push_back(connect(ui_->widgetMarketData, &MarketDataWidget::AskClicked,
      this, &RFQRequestWidget::onAskClicked));
   marketDataConnection_.push_back(connect(ui_->widgetMarketData, &MarketDataWidget::MDHeaderClicked,
      this, &RFQRequestWidget::onDisableSelectedInfo));
   marketDataConnection_.push_back(connect(ui_->widgetMarketData, &MarketDataWidget::clicked,
      this, &RFQRequestWidget::onRefreshFocus));

   userType_ = userType;
   ui_->shieldPage->showShieldSelectTargetTrade();
   popShield();
}

void RFQRequestWidget::onMatchingLogout()
{
   for (QMetaObject::Connection& conn : marketDataConnection_) {
      QObject::disconnect(conn);
   }
   for (const auto& dialog : dialogs_) {
      dialog.second->onMatchingLogout();
      dialog.second->deleteLater();
   }
   dialogs_.clear();
   userType_ = BaseCelerClient::CelerUserType::Undefined;
   ui_->shieldPage->showShieldLoginToSubmitRequired();
   popShield();
}

void RFQRequestWidget::onVerifiedAuthAddresses(const std::vector<bs::Address>& addrs)
{
   ui_->pageRFQTicket->onVerifiedAuthAddresses(addrs);
   forceCheckCondition();
}

void RFQRequestWidget::onAuthKey(const bs::Address& addr, const BinaryData& authKey)
{
   ui_->pageRFQTicket->onAuthKey(addr, authKey);
}

void RFQRequestWidget::onTradeSettings(const std::shared_ptr<bs::TradeSettings>& ts)
{
   ui_->pageRFQTicket->onTradeSettings(ts);
}

void RFQRequestWidget::onQuoteReceived(const bs::network::Quote& quote)
{
   const auto& itDlg = dialogs_.find(quote.requestId);
   if (itDlg != dialogs_.end()) {
      itDlg->second->onQuoteReceived(quote);
   }
}

void RFQRequestWidget::onQuoteMatched(const std::string& rfqId, const std::string& quoteId)
{
   const auto& itDlg = dialogs_.find(rfqId);
   if (itDlg != dialogs_.end()) {
      itDlg->second->onOrderFilled(quoteId);
   }
}

void RFQRequestWidget::onQuoteFailed(const std::string& rfqId
   , const std::string& quoteId, const std::string &info)
{
   const auto& itDlg = dialogs_.find(rfqId);
   if (itDlg != dialogs_.end()) {
      itDlg->second->onOrderFailed(quoteId, info);
   }
}

void RFQRequestWidget::onSettlementPending(const std::string& rfqId
   , const std::string& quoteId, const BinaryData& settlementId, int timeLeftMS)
{
   const auto& itDlg = dialogs_.find(rfqId);
   if (itDlg != dialogs_.end()) {
      itDlg->second->onSettlementPending(quoteId, settlementId);
   }
}

void RFQRequestWidget::onSettlementComplete(const std::string& rfqId
   , const std::string& quoteId, const BinaryData& settlementId)
{
   const auto& itDlg = dialogs_.find(rfqId);
   if (itDlg != dialogs_.end()) {
      itDlg->second->onSettlementComplete();
   } else {
      logger_->warn("[{}] RFQ dialog for {} not found", __func__, rfqId);
   }
}

void RFQRequestWidget::onReservedUTXOs(const std::string& resId
   , const std::string& subId, const std::vector<UTXO>& utxos)
{
   ui_->pageRFQTicket->onReservedUTXOs(resId, subId, utxos);
}

void RFQRequestWidget::hideEvent(QHideEvent* event)
{
   ui_->pageRFQTicket->onParentAboutToHide();
   QWidget::hideEvent(event);
}

bool RFQRequestWidget::eventFilter(QObject* sender, QEvent* event)
{
   if (QEvent::KeyPress == event->type() && ui_->pageRFQTicket->lineEditAmount() == sender) {
      QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
      if (Qt::Key_Up == keyEvent->key() || Qt::Key_Down == keyEvent->key()) {
         QKeyEvent *pEvent = new QKeyEvent(QEvent::KeyPress, keyEvent->key(), keyEvent->modifiers());
         QCoreApplication::postEvent(ui_->widgetMarketData->view(), pEvent);
         return true;
      }
   }
   return false;
}

void RFQRequestWidget::showEditableRFQPage()
{
   ui_->stackedWidgetRFQ->setEnabled(true);
   ui_->pageRFQTicket->enablePanel();
   ui_->stackedWidgetRFQ->setCurrentIndex(static_cast<int>(RFQPages::EditableRFQPage));
}

void RFQRequestWidget::showFuturesPage(bs::network::Asset::Type type)
{
   ui_->stackedWidgetRFQ->setEnabled(true);
   ui_->stackedWidgetRFQ->setCurrentIndex(static_cast<int>(RFQPages::Futures));
   ui_->pageFutures->setType(type);
}

void RFQRequestWidget::popShield()
{
   ui_->stackedWidgetRFQ->setEnabled(true);

   ui_->stackedWidgetRFQ->setCurrentIndex(static_cast<int>(RFQPages::ShieldPage));
   ui_->pageRFQTicket->disablePanel();
   ui_->widgetMarketData->view()->setFocus();
}

void RFQRequestWidget::init(const std::shared_ptr<spdlog::logger>&logger
   , const std::shared_ptr<DialogManager>& dialogMgr, OrderListModel* orderListModel)
{
   logger_ = logger;
   dialogManager_ = dialogMgr;
   ui_->pageRFQTicket->init(logger);

   ui_->treeViewOrders->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
   ui_->treeViewOrders->setModel(orderListModel);
   ui_->treeViewOrders->initWithModel(orderListModel);

   ui_->pageRFQTicket->disablePanel();
}

void RFQRequestWidget::onConnectedToCeler()
{
   marketDataConnection_.push_back(connect(ui_->widgetMarketData, &MarketDataWidget::CurrencySelected,
                                          this, &RFQRequestWidget::onCurrencySelected));
   marketDataConnection_.push_back(connect(ui_->widgetMarketData, &MarketDataWidget::BidClicked,
                                          this, &RFQRequestWidget::onBidClicked));
   marketDataConnection_.push_back(connect(ui_->widgetMarketData, &MarketDataWidget::AskClicked,
                                          this, &RFQRequestWidget::onAskClicked));
   marketDataConnection_.push_back(connect(ui_->widgetMarketData, &MarketDataWidget::MDHeaderClicked,
                                          this, &RFQRequestWidget::onDisableSelectedInfo));
   marketDataConnection_.push_back(connect(ui_->widgetMarketData, &MarketDataWidget::clicked,
                                          this, &RFQRequestWidget::onRefreshFocus));

   ui_->shieldPage->showShieldSelectTargetTrade();
   popShield();
}

void RFQRequestWidget::onDisconnectedFromCeler()
{
   for (QMetaObject::Connection &conn : marketDataConnection_) {
      QObject::disconnect(conn);
   }

   ui_->shieldPage->showShieldLoginToSubmitRequired();
   popShield();
}

void RFQRequestWidget::onRFQSubmit(const std::string &id, const bs::network::RFQ& rfq
   , bs::UtxoReservationToken ccUtxoRes)
{
   auto authAddr = ui_->pageRFQTicket->selectedAuthAddress();
   RFQDialog* dialog = nullptr;
   auto fixedXbtInputs = ui_->pageRFQTicket->fixedXbtInputs();
   bs::hd::Purpose purpose = bs::hd::Purpose::Unknown;

   if (walletsManager_) {
      auto xbtWallet = ui_->pageRFQTicket->xbtWallet();

      if (xbtWallet && !xbtWallet->canMixLeaves()) {
         auto walletType = ui_->pageRFQTicket->xbtWalletType();
         purpose = UiUtils::getHwWalletPurpose(walletType);
      }

      dialog = new RFQDialog(logger_, id, rfq, quoteProvider_
         , authAddressManager_, assetManager_, walletsManager_, signingContainer_
         , armory_, celerClient_, appSettings_, rfqStorage_, xbtWallet
         , ui_->pageRFQTicket->recvXbtAddressIfSet(), authAddr, utxoReservationManager_
         , fixedXbtInputs.inputs, std::move(fixedXbtInputs.utxoRes)
         , std::move(ccUtxoRes), purpose, this);
   }
   else {
      std::string xbtWalletId;
      dialog = new RFQDialog(logger_, id, rfq, xbtWalletId
         , ui_->pageRFQTicket->recvXbtAddressIfSet(), authAddr, purpose, this);
      const std::string reserveId = (rfq.assetType == bs::network::Asset::SpotFX) ?
         "" : rfq.requestId;
      emit needSubmitRFQ(rfq, reserveId);
   }

   connect(this, &RFQRequestWidget::unsignedPayinRequested, dialog, &RFQDialog::onUnsignedPayinRequested);
   connect(this, &RFQRequestWidget::signedPayoutRequested, dialog, &RFQDialog::onSignedPayoutRequested);
   connect(this, &RFQRequestWidget::signedPayinRequested, dialog, &RFQDialog::onSignedPayinRequested);
   connect(dialog, &RFQDialog::accepted, this, &RFQRequestWidget::onRFQAccepted);
   connect(dialog, &RFQDialog::expired, this, &RFQRequestWidget::onRFQExpired);
   connect(dialog, &RFQDialog::cancelled, this, &RFQRequestWidget::onRFQCancelled);

   dialogManager_->adjustDialogPosition(dialog);
   dialog->show();

   const auto &itDlg = dialogs_.find(id);
   if (itDlg != dialogs_.end()) {   //np, most likely a resend from script
      itDlg->second->deleteLater();
      itDlg->second = dialog;
   }
   else {
      dialogs_[id] = dialog;
   }
   ui_->pageRFQTicket->resetTicket();
   for (const auto& bal : balances_) {
      dialog->onBalance(bal.first, bal.second);
   }

   const auto& currentInfo = ui_->widgetMarketData->getCurrentlySelectedInfo();
   ui_->pageRFQTicket->SetProductAndSide(currentInfo.productGroup_
      , currentInfo.currencyPair_, currentInfo.bidPrice_, currentInfo.offerPrice_
      , bs::network::Side::Undefined);
   ui_->pageFutures->SetProductAndSide(currentInfo.productGroup_
      , currentInfo.currencyPair_, currentInfo.bidPrice_, currentInfo.offerPrice_
      , bs::network::Side::Undefined);

   std::vector<std::string> closedDialogs;
   for (const auto &dlg : dialogs_) {
      if (dlg.second->isHidden()) {
         dlg.second->deleteLater();
         closedDialogs.push_back(dlg.first);
      }
   }
   for (const auto &dlg : closedDialogs) {
      dialogs_.erase(dlg);
   }
}

void RFQRequestWidget::onRFQCancel(const std::string &id)
{
   deleteDialog(id);
}

void RFQRequestWidget::deleteDialog(const std::string &rfqId)
{
   const auto &itDlg = dialogs_.find(rfqId);
   if (itDlg == dialogs_.end()) {
      return;
   }
   itDlg->second->cancel();
   itDlg->second->deleteLater();
   dialogs_.erase(itDlg);
}

void RFQRequestWidget::processFutureResponse(const ProxyTerminalPb::Response_FutureResponse &msg)
{
   QMetaObject::invokeMethod(this, [this, msg] {
      if (!msg.success()) {
         BSMessageBox errorMessage(BSMessageBox::critical, tr("Order message")
            , tr("Trade rejected"), QString::fromStdString(msg.error_msg()), this);
         errorMessage.exec();
         return;
      }
      auto productStr = QString::fromStdString(msg.product().empty() ? "<Unknown>" : msg.product());
      auto sideStr = msg.side() == bs::types::Side::SIDE_SELL ? tr("Sell") : tr("Buy");
      auto amountStr = UiUtils::displayAmount(msg.amount());
      auto priceStr = UiUtils::displayPriceXBT(msg.price());
      auto details = tr("Product:\t%1\nSide:\t%2\nVolume:\t%3\nPrice:\t%4")
         .arg(productStr).arg(sideStr).arg(amountStr).arg(priceStr);
      BSMessageBox errorMessage(BSMessageBox::info, tr("Order message"), tr("Order confirmation"), details, this);
      errorMessage.exec();
   });
}

bool RFQRequestWidget::checkConditions(const MarketSelectedInfo& selectedInfo)
{
   ui_->stackedWidgetRFQ->setEnabled(true);
   using UserType = CelerClient::CelerUserType;
   const UserType userType = celerClient_ ? celerClient_->celerUserType() : userType_;

   const auto group = RFQShieldPage::getProductGroup(selectedInfo.productGroup_);

   if (group == WalletShieldBase::ProductType::CashSettledFutures
      || group == WalletShieldBase::ProductType::DeliverableFutures) {
      showFuturesPage(group);
      return true;
   }

   switch (userType) {
   case UserType::Market: {
      if (group == WalletShieldBase::ProductType::SpotFX || group == WalletShieldBase::ProductType::SpotXBT) {
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
      if ((group == WalletShieldBase::ProductType::SpotXBT || group == WalletShieldBase::ProductType::PrivateMarket) &&
         checkWalletSettings(group, selectedInfo)) {
         return false;
      }
      break;
   }
   default: break;
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
   if (celerClient_) {
      if (!ui_->widgetMarketData || !celerClient_->IsConnected()) {
         return;
      }
   }

   const auto& currentInfo = ui_->widgetMarketData->getCurrentlySelectedInfo();
   if (!currentInfo.isValid()) {
      return;
   }
   onCurrencySelected(currentInfo);
}

void RFQRequestWidget::onCurrencySelected(const MarketSelectedInfo& selectedInfo)
{
   if (!checkConditions(selectedInfo)) {
      return;
   }
   ui_->pageRFQTicket->setSecurityId(selectedInfo.productGroup_
      , selectedInfo.currencyPair_, selectedInfo.bidPrice_, selectedInfo.offerPrice_);
   ui_->pageFutures->setSecurityId(selectedInfo.productGroup_
      , selectedInfo.currencyPair_, selectedInfo.bidPrice_, selectedInfo.offerPrice_);
}

void RFQRequestWidget::onBidClicked(const MarketSelectedInfo& selectedInfo)
{
   if (!checkConditions(selectedInfo)) {
      return;
   }
   ui_->pageRFQTicket->setSecuritySell(selectedInfo.productGroup_
      , selectedInfo.currencyPair_, selectedInfo.bidPrice_, selectedInfo.offerPrice_);
   ui_->pageFutures->setSecuritySell(selectedInfo.productGroup_
      , selectedInfo.currencyPair_, selectedInfo.bidPrice_, selectedInfo.offerPrice_);
}

void RFQRequestWidget::onAskClicked(const MarketSelectedInfo& selectedInfo)
{
   if (!checkConditions(selectedInfo)) {
      return;
   }
   ui_->pageRFQTicket->setSecurityBuy(selectedInfo.productGroup_
      , selectedInfo.currencyPair_, selectedInfo.bidPrice_, selectedInfo.offerPrice_);
   ui_->pageFutures->setSecurityBuy(selectedInfo.productGroup_
      , selectedInfo.currencyPair_, selectedInfo.bidPrice_, selectedInfo.offerPrice_);
}

void RFQRequestWidget::onDisableSelectedInfo()
{
   ui_->shieldPage->showShieldSelectTargetTrade();
   popShield();
}

void RFQRequestWidget::onRefreshFocus()
{
   if (ui_->stackedWidgetRFQ->currentIndex() == static_cast<int>(RFQPages::EditableRFQPage)) {
      ui_->pageRFQTicket->lineEditAmount()->setFocus();
   }
}

void RFQRequestWidget::onMessageFromPB(const Blocksettle::Communication::ProxyTerminalPb::Response &response)
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
         const auto &command = response.sign_payin();
         auto timestamp = QDateTime::fromMSecsSinceEpoch(command.timestamp_ms());
         // unsigned_payin_data - serialized payin. binary
         emit signedPayinRequested(command.settlement_id(), BinaryData::fromString(command.unsigned_payin_data())
            , BinaryData::fromString(command.payin_hash()), timestamp);
         break;
      }

      case Blocksettle::Communication::ProxyTerminalPb::Response::kFutureResponse: {
         processFutureResponse(response.future_response());
         break;
      }

      default:
         break;
   }
   // if not processed - not RFQ releated message. not error
}

void RFQRequestWidget::onUserConnected(const bs::network::UserType &ut)
{
   if (appSettings_ && appSettings_->get<bool>(ApplicationSettings::AutoStartRFQScript)) {
      QTimer::singleShot(1000, [this] { // add some delay to allow initial sync of data
         ((RFQScriptRunner *)autoSignProvider_->scriptRunner())->start(
            autoSignProvider_->getLastScript());
      });
   }
}

void RFQRequestWidget::onUserDisconnected()
{
   if (autoSignProvider_) {
      ((RFQScriptRunner*)autoSignProvider_->scriptRunner())->suspend();
   }
}

void RFQRequestWidget::onRFQAccepted(const std::string &id
   , const bs::network::Quote& quote)
{
   if (autoSignProvider_) {
      ((RFQScriptRunner*)autoSignProvider_->scriptRunner())->rfqAccepted(id);
   }
   else {
      emit needAcceptRFQ(id, quote);
   }
}

void RFQRequestWidget::onRFQExpired(const std::string &id)
{
   deleteDialog(id);
   if (autoSignProvider_) {
      ((RFQScriptRunner*)autoSignProvider_->scriptRunner())->rfqExpired(id);
   }
   else {
      emit needExpireRFQ(id);
   }
}

void RFQRequestWidget::onRFQCancelled(const std::string &id)
{
   if (autoSignProvider_) {
      ((RFQScriptRunner*)autoSignProvider_->scriptRunner())->rfqCancelled(id);
   }
   else {
      emit needCancelRFQ(id);
   }
}

void RFQRequestWidget::onOrderClicked(const QModelIndex &index)
{
   if (!index.isValid()) {
      return;
   }

   if (orderListModel_->DeliveryRequired(index)) {
      emit CreateObligationDeliveryTX(index);
   }
}
