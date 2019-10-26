#ifndef __RFQ_REQUEST_WIDGET_H__
#define __RFQ_REQUEST_WIDGET_H__

#include <QWidget>
#include <QTimer>
#include <memory>

#include "CommonTypes.h"
#include "MarketDataWidget.h"
#include "TabWithShortcut.h"

namespace Ui {
    class RFQRequestWidget;
}
namespace spdlog {
   class logger;
}
namespace bs {
   namespace sync {
      class WalletsManager;
   }
}

namespace Blocksettle {
   namespace Communication {
      namespace ProxyTerminalPb {
         class Response;
      }
   }
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
class RfqStorage;
class SignContainer;

class RFQRequestWidget : public TabWithShortcut
{
Q_OBJECT

public:
   RFQRequestWidget(QWidget* parent = nullptr);
   ~RFQRequestWidget() override;

   void initWidgets(const std::shared_ptr<MarketDataProvider>& mdProvider
      , const std::shared_ptr<ApplicationSettings> &appSettings);

   void init(std::shared_ptr<spdlog::logger> logger
         , const std::shared_ptr<BaseCelerClient>& celerClient
         , const std::shared_ptr<AuthAddressManager> &
         , std::shared_ptr<QuoteProvider> quoteProvider
         , const std::shared_ptr<AssetManager>& assetManager
         , const std::shared_ptr<DialogManager> &dialogManager
         , const std::shared_ptr<SignContainer> &
         , const std::shared_ptr<ArmoryConnection> &
         , const std::shared_ptr<ConnectionManager> &connectionManager
         , OrderListModel *orderListModel);

   void setWalletsManager(const std::shared_ptr<bs::sync::WalletsManager> &);

   void shortcutActivated(ShortcutType s) override;

   void setAuthorized(bool authorized);

signals:
   void requestPrimaryWalletCreation();

   void sendUnsignedPayinToPB(const std::string& settlementId, const BinaryData& unsignedPayin, const BinaryData& unsignedTxId);
   void sendSignedPayinToPB(const std::string& settlementId, const BinaryData& signedPayin);
   void sendSignedPayoutToPB(const std::string& settlementId, const BinaryData& signedPayout);

   void unsignedPayinRequested(const std::string& settlementId);
   void signedPayoutRequested(const std::string& settlementId, const BinaryData& payinHash);
   void signedPayinRequested(const std::string& settlementId, const BinaryData& unsignedPayin);

private:
   void showEditableRFQPage();
   void popShield();

   bool checkConditions(const MarketSelectedInfo& productGroup);
   bool checkWalletSettings(bs::network::Asset::Type productType, const MarketSelectedInfo& productGroup);

public slots:
   void onRFQSubmit(const bs::network::RFQ& rfq);
   void onCurrencySelected(const MarketSelectedInfo& selectedInfo);
   void onBidClicked(const MarketSelectedInfo& selectedInfo);
   void onAskClicked(const MarketSelectedInfo& selectedInfo);
   void onDisableSelectedInfo();

   void onMessageFromPB(const Blocksettle::Communication::ProxyTerminalPb::Response &response);

private slots:
   void onConnectedToCeler();
   void onDisconnectedFromCeler();

public slots:
   void forceCheckCondition();

private:
   std::unique_ptr<Ui::RFQRequestWidget> ui_;

   std::shared_ptr<spdlog::logger>     logger_;
   std::shared_ptr<BaseCelerClient>        celerClient_;
   std::shared_ptr<QuoteProvider>      quoteProvider_;
   std::shared_ptr<AssetManager>       assetManager_;
   std::shared_ptr<AuthAddressManager> authAddressManager_;
   std::shared_ptr<DialogManager>      dialogManager_;

   std::shared_ptr<bs::sync::WalletsManager> walletsManager_;
   std::shared_ptr<SignContainer>      signingContainer_;
   std::shared_ptr<ArmoryConnection>   armory_;
   std::shared_ptr<ApplicationSettings> appSettings_;
   std::shared_ptr<ConnectionManager>  connectionManager_;

   std::shared_ptr<RfqStorage> rfqStorage_;

   QList<QMetaObject::Connection>   marketDataConnection;
};

#endif // __RFQ_REQUEST_WIDGET_H__
