/*

***********************************************************************************
* Copyright (C) 2019 - 2021, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __RFQ_REPLY_WIDGET_H__
#define __RFQ_REPLY_WIDGET_H__

#include <QWidget>
#include <QTimer>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <tuple>
#include "ApplicationSettings.h"
#include "BSErrorCode.h"
#include "CoinControlModel.h"
#include "CommonTypes.h"
#include "HDPath.h"
#include "SignerDefs.h"
#include "TabWithShortcut.h"
#include "UtxoReservationToken.h"

#include <QPushButton>
#include <QStyledItemDelegate>
#include <QPainter>
#include <QRect>

namespace Ui {
    class RFQReplyWidget;
}
namespace spdlog {
   class logger;
}
namespace bs {
   namespace sync {
      namespace hd {
         class Wallet;
      }
      class WalletsManager;
   }
   class SettlementAddressEntry;
   class SecurityStatsCollector;
   class UTXOReservationManager;
}
class ApplicationSettings;
class ArmoryConnection;
class AssetManager;
class AuthAddressManager;
class AutoSignScriptProvider;
class CelerClientQt;
class ConnectionManager;
class MDCallbacksQt;
class OrderListModel;
class QuoteProvider;

namespace Blocksettle {
   namespace Communication {
      namespace ProxyTerminalPb {
         class Response;
         class Response_UpdateOrders;
      }
   }
}

namespace bs {
   namespace ui {
      struct SubmitQuoteReplyData;
   }
}

class RFQReplyWidget : public TabWithShortcut
{
Q_OBJECT

public:
   RFQReplyWidget(QWidget* parent = nullptr);
   ~RFQReplyWidget() override;

   [[deprecated]] void init(const std::shared_ptr<spdlog::logger> &
      , const std::shared_ptr<CelerClientQt> &
      , const std::shared_ptr<AuthAddressManager> &
      , const std::shared_ptr<QuoteProvider> &
      , const std::shared_ptr<MDCallbacksQt> &
      , const std::shared_ptr<AssetManager> &
      , const std::shared_ptr<ApplicationSettings> &
      , const std::shared_ptr<WalletSignerContainer> &
      , const std::shared_ptr<ArmoryConnection> &
      , const std::shared_ptr<ConnectionManager> &
      , const std::shared_ptr<AutoSignScriptProvider> &
      , const std::shared_ptr<bs::UTXOReservationManager> &
      , OrderListModel *orderListModel);
   void init(const std::shared_ptr<spdlog::logger>&, OrderListModel*);
   [[deprecated]] void setWalletsManager(const std::shared_ptr<bs::sync::WalletsManager> &);

   void shortcutActivated(ShortcutType s) override;

   void onMatchingLogout();
   void onMDUpdated(bs::network::Asset::Type, const QString& security
      , const bs::network::MDFields&);
   void onBalance(const std::string& currency, double balance);
   void onWalletBalance(const bs::sync::WalletBalanceData&);
   void onHDWallet(const bs::sync::HDWalletData&);
   void onAuthKey(const bs::Address&, const BinaryData& authKey);
   void onVerifiedAuthAddresses(const std::vector<bs::Address>&);
   void onReservedUTXOs(const std::string& resId, const std::string& subId
      , const std::vector<UTXO>&);

   void onQuoteReqNotification(const bs::network::QuoteReqNotification&);
   void onQuoteMatched(const std::string& rfqId, const std::string& quoteId);
   void onQuoteFailed(const std::string& rfqId, const std::string& quoteId
      , const std::string& info);
   void onSettlementPending(const std::string& rfqId, const std::string& quoteId
      , const BinaryData& settlementId, int timeLeftMS);
   void onSettlementComplete(const std::string& rfqId, const std::string& quoteId
      , const BinaryData& settlementId);

signals:
   void orderFilled();
   void requestPrimaryWalletCreation();

   void sendUnsignedPayinToPB(const std::string& settlementId, const bs::network::UnsignedPayinData& unsignedPayinData);
   void sendSignedPayinToPB(const std::string& settlementId, const BinaryData& signedPayin);
   void sendSignedPayoutToPB(const std::string& settlementId, const BinaryData& signedPayout);

   void cancelXBTTrade(const std::string& settlementId);
   void cancelCCTrade(const std::string& clientOrderId);

   void unsignedPayinRequested(const std::string& settlementId);
   void signedPayoutRequested(const std::string& settlementId, const BinaryData& payinHash, QDateTime timestamp);
   void signedPayinRequested(const std::string& settlementId, const BinaryData& unsignedPayin
      , const BinaryData &payinHash, QDateTime timestamp);

   void CreateObligationDeliveryTX(const QModelIndex& index);

   void submitQuote(const bs::network::QuoteNotification&);
   void pullQuote(const std::string& settlementId, const std::string& reqId
      , const std::string& reqSessToken);

   void putSetting(ApplicationSettings::Setting, const QVariant&);
   void needAuthKey(const bs::Address&);
   void needReserveUTXOs(const std::string& reserveId, const std::string& subId
      , uint64_t amount, bool withZC = false, const std::vector<UTXO>& utxos = {});

public slots:
   void forceCheckCondition();

   void onMessageFromPB(const Blocksettle::Communication::ProxyTerminalPb::Response &response);
   void onUserConnected(const bs::network::UserType &);
   void onQuoteCancelled(const QString& reqId, bool userCancelled);
   void onQuoteNotifCancelled(const QString& reqId);

private slots:
   void onOrder(const bs::network::Order &o);
   void onQuoteRejected(const QString &reqId, const QString &reason);

   void saveTxData(QString orderId, std::string txData);
   void onSignTxRequested(QString orderId, QString reqId, QDateTime timestamp);
   void onConnectedToCeler();
   void onDisconnectedFromCeler();
   void onEnterKeyPressed(const QModelIndex &index);
   void onSelected(const QString& productGroup, const bs::network::QuoteReqNotification& request, double indicBid, double indicAsk);
   void onTransactionError(const std::string &id, bs::error::ErrorCode code, const QString& error);

   void onReplied(const std::shared_ptr<bs::ui::SubmitQuoteReplyData> &data);
   void onPulled(const std::string& settlementId, const std::string& reqId, const std::string& reqSessToken);

   void onCancelXBTTrade(const std::string& settlementId);
   void onCancelCCTrade(const std::string& clientOrderId);
   void onCompleteSettlement(const std::string &id);

   void onOrderClicked(const QModelIndex &index);

private:
   void onResetCurrentReservation(const std::shared_ptr<bs::ui::SubmitQuoteReplyData> &data);
   bool checkConditions(const QString& productGroup, const bs::network::QuoteReqNotification& request);
   void popShield();
   void showEditableRFQPage();
   void eraseReply(const QString &reqId);

protected:
   void hideEvent(QHideEvent* event) override;

private:
   struct SentXbtReply
   {
      std::shared_ptr<bs::sync::hd::Wallet> xbtWallet;
      bs::Address authAddr;
      std::vector<UTXO>          utxosPayinFixed;
      bs::UtxoReservationToken   utxoRes;
      bs::hd::Purpose            walletPurpose;
   };

   struct SentCCReply
   {
      std::string                         recipientAddress;
      std::string                         requestorAuthAddress;
      std::shared_ptr<bs::sync::hd::Wallet>  xbtWallet;
      bs::UtxoReservationToken            utxoRes;
      bs::hd::Purpose                     walletPurpose;
   };

private:
   std::unique_ptr<Ui::RFQReplyWidget>    ui_;
   std::shared_ptr<spdlog::logger>        logger_;
   std::shared_ptr<CelerClientQt>         celerClient_;
   std::shared_ptr<QuoteProvider>         quoteProvider_;
   std::shared_ptr<AuthAddressManager>    authAddressManager_;
   std::shared_ptr<AssetManager>          assetManager_;
   std::shared_ptr<bs::sync::WalletsManager> walletsManager_;
   std::shared_ptr<WalletSignerContainer> signingContainer_;
   std::shared_ptr<ArmoryConnection>      armory_;
   std::shared_ptr<ApplicationSettings>   appSettings_;
   std::shared_ptr<ConnectionManager>     connectionManager_;
   std::shared_ptr<AutoSignScriptProvider>      autoSignProvider_;
   std::shared_ptr<bs::UTXOReservationManager>  utxoReservationManager_;
   OrderListModel*                              orderListModel_;

   std::unordered_map<std::string, SentXbtReply>   sentXbtReplies_;
   std::unordered_map<std::string, SentCCReply>    sentCCReplies_;
   std::shared_ptr<bs::SecurityStatsCollector>     statsCollector_;
   std::unordered_map<std::string, std::string>    sentReplyToSettlementsIds_, settlementToReplyIds_;

   bs::network::UserType   userType_{ bs::network::UserType::Undefined };
   std::unordered_map<std::string, bs::network::Asset::Type>   submittedQuote_;
};

#include <QDebug>

class PushButtonDelegate : public QStyledItemDelegate
{
   Q_OBJECT

public:
   explicit PushButtonDelegate(QWidget* parent)
   {
      button_ = new QPushButton(tr("Submit"), parent);
      button_->hide();
   }
   ~PushButtonDelegate() override = default;

   QSize sizeHint(const QStyleOptionViewItem &option,const QModelIndex &index) const override
   {
      auto statusGroupIndex = index;
      int depth = 0;
      while (statusGroupIndex.parent().isValid()) {
         statusGroupIndex = statusGroupIndex.parent();
         ++depth;
      }

      if (statusGroupIndex.row() != 2 || depth != 1) {
         return QStyledItemDelegate::sizeHint(option, index);
      }

      auto minSizeHint = button_->minimumSizeHint();
      auto minSize = button_->minimumSize();
      auto sizeHint = button_->sizeHint();

      return sizeHint;
   }

   void paint(QPainter* painter, const QStyleOptionViewItem& opt,
      const QModelIndex& index) const override
   {
      auto statusGroupIndex = index;
      int depth = 0;
      while (statusGroupIndex.parent().isValid()) {
         statusGroupIndex = statusGroupIndex.parent();
         ++depth;
      }

      if (statusGroupIndex.row() != 2 || depth != 2) {
         QStyledItemDelegate::paint(painter, opt, index);
         return;
      }

      auto minSizeHint = button_->minimumSizeHint();
      auto rect = opt.rect;

      if (rect.width() < minSizeHint.width()) {
         rect.setWidth(minSizeHint.width());
      }

      if (rect.height() < minSizeHint.height()) {
         rect.setHeight(minSizeHint.height());
      }

      button_->setGeometry(rect);

      qDebug() << "PushButtonDelegate " << opt.state;

      QPixmap map = button_->grab();
      painter->drawPixmap(rect.x(), rect.y(), map);
   }

private:
   QPushButton *button_;
};

#endif // __RFQ_REPLY_WIDGET_H__
