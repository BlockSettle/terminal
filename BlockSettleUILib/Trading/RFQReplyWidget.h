/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
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

#include "AutoSignQuoteProvider.h"
#include "TransactionData.h"
#include "CoinControlModel.h"
#include "CommonTypes.h"
#include "TabWithShortcut.h"
#include "UtxoReservationToken.h"

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
}
class ApplicationSettings;
class ArmoryConnection;
class AssetManager;
class AuthAddressManager;
class BaseCelerClient;
class ConnectionManager;
class DialogManager;
class MarketDataProvider;
class OrderListModel;
class QuoteProvider;
class SignContainer;

namespace Blocksettle {
   namespace Communication {
      namespace ProxyTerminalPb {
         class Response;
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
      , const std::shared_ptr<MarketDataProvider> &
      , const std::shared_ptr<AssetManager> &
      , const std::shared_ptr<ApplicationSettings> &
      , const std::shared_ptr<DialogManager> &
      , const std::shared_ptr<SignContainer> &
      , const std::shared_ptr<ArmoryConnection> &
      , const std::shared_ptr<ConnectionManager> &
      , const std::shared_ptr<AutoSignQuoteProvider> &
      , OrderListModel *orderListModel);

   void setWalletsManager(const std::shared_ptr<bs::sync::WalletsManager> &);

   void shortcutActivated(ShortcutType s) override;

signals:
   void orderFilled();
   void requestPrimaryWalletCreation();

   void sendUnsignedPayinToPB(const std::string& settlementId, const bs::network::UnsignedPayinData& unsignedPayinData);
   void sendSignedPayinToPB(const std::string& settlementId, const BinaryData& signedPayin);
   void sendSignedPayoutToPB(const std::string& settlementId, const BinaryData& signedPayout);

   void unsignedPayinRequested(const std::string& settlementId);
   void signedPayoutRequested(const std::string& settlementId, const BinaryData& payinHash);
   void signedPayinRequested(const std::string& settlementId, const BinaryData& unsignedPayin);

public slots:
   void forceCheckCondition();

   void onMessageFromPB(const Blocksettle::Communication::ProxyTerminalPb::Response &response);

private slots:
   void onOrder(const bs::network::Order &o);
   void saveTxData(QString orderId, std::string txData);
   void onSignTxRequested(QString orderId, QString reqId);
   void onConnectedToCeler();
   void onDisconnectedFromCeler();
   void onEnterKeyPressed(const QModelIndex &index);
   void onSelected(const QString& productGroup, const bs::network::QuoteReqNotification& request, double indicBid, double indicAsk);

private:
   void onReplied(const std::shared_ptr<bs::ui::SubmitQuoteReplyData> &data);
   void showSettlementDialog(QDialog *dlg);
   bool checkConditions(const QString& productGroup, const bs::network::QuoteReqNotification& request);
   void popShield();
   void showEditableRFQPage();

private:
   using transaction_data_ptr = std::shared_ptr<TransactionData>;
   using settl_addr_ptr = std::shared_ptr<bs::SettlementAddressEntry>;

   struct SentXbtReply
   {
      std::shared_ptr<bs::sync::hd::Wallet> xbtWallet;
      bs::Address authAddr;
      std::vector<UTXO> utxosPayinFixed;
   };

   struct SentCCReply
   {
      std::string                         recipientAddress;
      std::string                         requestorAuthAddress;
      std::shared_ptr<bs::sync::hd::Wallet>  xbtWallet;
      bs::UtxoReservationToken            utxoRes;
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
   std::shared_ptr<SignContainer>         signingContainer_;
   std::shared_ptr<ArmoryConnection>      armory_;
   std::shared_ptr<ApplicationSettings>   appSettings_;
   std::shared_ptr<ConnectionManager>     connectionManager_;
   std::shared_ptr<AutoSignQuoteProvider>    autoSignQuoteProvider_;

   std::unordered_map<std::string, SentXbtReply>   sentXbtReplies_;
   std::unordered_map<std::string, SentCCReply>    sentCCReplies_;
   std::shared_ptr<bs::SecurityStatsCollector>     statsCollector_;
};

#endif // __RFQ_REPLY_WIDGET_H__
