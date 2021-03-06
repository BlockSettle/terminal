/*

***********************************************************************************
* Copyright (C) 2019 - 2020, BlockSettle AB
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

#include "BSErrorCode.h"
#include "CoinControlModel.h"
#include "CommonTypes.h"
#include "TabWithShortcut.h"
#include "UtxoReservationToken.h"
#include "HDPath.h"

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
class BaseCelerClient;
class ConnectionManager;
class DialogManager;
class MDCallbacksQt;
class OrderListModel;
class QuoteProvider;
class WalletSignerContainer;

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

   void init(const std::shared_ptr<spdlog::logger> &
      , const std::shared_ptr<BaseCelerClient> &
      , const std::shared_ptr<AuthAddressManager> &
      , const std::shared_ptr<QuoteProvider> &
      , const std::shared_ptr<MDCallbacksQt> &
      , const std::shared_ptr<AssetManager> &
      , const std::shared_ptr<ApplicationSettings> &
      , const std::shared_ptr<DialogManager> &
      , const std::shared_ptr<WalletSignerContainer> &
      , const std::shared_ptr<ArmoryConnection> &
      , const std::shared_ptr<ConnectionManager> &
      , const std::shared_ptr<AutoSignScriptProvider> &
      , const std::shared_ptr<bs::UTXOReservationManager> &
      , OrderListModel *orderListModel);

   void setWalletsManager(const std::shared_ptr<bs::sync::WalletsManager> &);

   void shortcutActivated(ShortcutType s) override;

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

public slots:
   void forceCheckCondition();

   void onMessageFromPB(const Blocksettle::Communication::ProxyTerminalPb::Response &response);
   void onUserConnected(const bs::network::UserType &);

private slots:
   void onOrder(const bs::network::Order &o);
   void onQuoteCancelled(const QString &reqId, bool userCancelled);
   void onQuoteRejected(const QString &reqId, const QString &reason);
   void onQuoteNotifCancelled(const QString &reqId);

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
   void onSettlementComplete(const std::string &id);

private:
   void onResetCurrentReservation(const std::shared_ptr<bs::ui::SubmitQuoteReplyData> &data);
   void showSettlementDialog(QDialog *dlg);
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
      std::vector<UTXO> utxosPayinFixed;
      bs::UtxoReservationToken utxoRes;
      std::unique_ptr<bs::hd::Purpose> walletPurpose;
   };

   struct SentCCReply
   {
      std::string                         recipientAddress;
      std::string                         requestorAuthAddress;
      std::shared_ptr<bs::sync::hd::Wallet>  xbtWallet;
      bs::UtxoReservationToken            utxoRes;
      std::unique_ptr<bs::hd::Purpose> walletPurpose;
   };

private:
   std::unique_ptr<Ui::RFQReplyWidget>    ui_;
   std::shared_ptr<spdlog::logger>        logger_;
   std::shared_ptr<BaseCelerClient>       celerClient_;
   std::shared_ptr<QuoteProvider>         quoteProvider_;
   std::shared_ptr<AuthAddressManager>    authAddressManager_;
   std::shared_ptr<AssetManager>          assetManager_;
   std::shared_ptr<bs::sync::WalletsManager> walletsManager_;
   std::shared_ptr<DialogManager>         dialogManager_;
   std::shared_ptr<WalletSignerContainer> signingContainer_;
   std::shared_ptr<ArmoryConnection>      armory_;
   std::shared_ptr<ApplicationSettings>   appSettings_;
   std::shared_ptr<ConnectionManager>     connectionManager_;
   std::shared_ptr<AutoSignScriptProvider>      autoSignProvider_;
   std::shared_ptr<bs::UTXOReservationManager>  utxoReservationManager_;

   std::unordered_map<std::string, SentXbtReply>   sentXbtReplies_;
   std::unordered_map<std::string, SentCCReply>    sentCCReplies_;
   std::shared_ptr<bs::SecurityStatsCollector>     statsCollector_;
   std::unordered_map<std::string, std::string>    sentReplyToSettlementsIds_, settlementToReplyIds_;
};

#endif // __RFQ_REPLY_WIDGET_H__
