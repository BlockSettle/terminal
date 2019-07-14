#ifndef __RFQ_REPLY_WIDGET_H__
#define __RFQ_REPLY_WIDGET_H__

#include <QWidget>
#include <QTimer>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <tuple>

#include "TransactionData.h"
#include "CoinControlModel.h"
#include "CommonTypes.h"
#include "TabWithShortcut.h"

namespace Ui {
    class RFQReplyWidget;
}
namespace spdlog {
   class logger;
}
namespace bs {
   namespace sync {
      class WalletsManager;
   }
   class SettlementAddressEntry;
   class SecurityStatsCollector;
}
class ArmoryConnection;
class AssetManager;
class AuthAddressManager;
class BaseCelerClient;
class DialogManager;
class MarketDataProvider;
class QuoteProvider;
class SignContainer;
class ApplicationSettings;
class ConnectionManager;


class RFQReplyWidget : public TabWithShortcut
{
Q_OBJECT

public:
   RFQReplyWidget(QWidget* parent = nullptr);
   ~RFQReplyWidget() override;

   void init(std::shared_ptr<spdlog::logger> logger
      , const std::shared_ptr<BaseCelerClient>& celerClient
      , const std::shared_ptr<AuthAddressManager> &
      , const std::shared_ptr<QuoteProvider>& quoteProvider
      , const std::shared_ptr<MarketDataProvider>& mdProvider
      , const std::shared_ptr<AssetManager>& assetManager
      , const std::shared_ptr<ApplicationSettings> &appSettings
      , const std::shared_ptr<DialogManager> &dialogManager
      , const std::shared_ptr<SignContainer> &
      , const std::shared_ptr<ArmoryConnection> &
      , const std::shared_ptr<ConnectionManager> &connectionManager);

   void setWalletsManager(const std::shared_ptr<bs::sync::WalletsManager> &);

   void shortcutActivated(ShortcutType s) override;

signals:
   void orderFilled();

private slots:
   void onReplied(bs::network::QuoteNotification qn);
   void onOrder(const bs::network::Order &o);
   void saveTxData(QString orderId, std::string txData);
   void onSignTxRequested(QString orderId, QString reqId);
   void onReadyToAutoSign();
   void onAutoSignActivated(const QString &hdWalletId, bool active);
   void onConnectedToCeler();
   void onDisconnectedFromCeler();

private:
   void showSettlementDialog(QDialog *dlg);

private:
   using transaction_data_ptr = std::shared_ptr<TransactionData>;
   using settl_addr_ptr = std::shared_ptr<bs::SettlementAddressEntry>;

   struct SentCCReply
   {
      std::string                      recipientAddress;
      std::shared_ptr<TransactionData> txData;
      std::string                      requestorAuthAddress;
   };

private:
   std::unique_ptr<Ui::RFQReplyWidget> ui_;
   std::shared_ptr<spdlog::logger>        logger_;
   std::shared_ptr<BaseCelerClient>           celerClient_;
   std::shared_ptr<QuoteProvider>         quoteProvider_;
   std::shared_ptr<AuthAddressManager>    authAddressManager_;
   std::shared_ptr<AssetManager>          assetManager_;
   std::shared_ptr<bs::sync::WalletsManager> walletsManager_;
   std::shared_ptr<DialogManager>         dialogManager_;
   std::shared_ptr<SignContainer>         signingContainer_;
   std::shared_ptr<ArmoryConnection>      armory_;
   std::shared_ptr<ApplicationSettings>   appSettings_;
   std::shared_ptr<ConnectionManager>     connectionManager_;

   std::unordered_map<std::string, transaction_data_ptr>   sentXbtTransactionData_;
   std::unordered_map<std::string, SentCCReply>    sentCCReplies_;
   std::shared_ptr<bs::SecurityStatsCollector>     statsCollector_;
   std::unordered_map<std::string, std::string>    ccTxByOrder_;
};

#endif // __RFQ_REPLY_WIDGET_H__
