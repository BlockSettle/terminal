/*

***********************************************************************************
* Copyright (C) 2019 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __RFQ_REQUEST_WIDGET_H__
#define __RFQ_REQUEST_WIDGET_H__

#include <QWidget>
#include <QTimer>
#include <memory>

#include "CommonTypes.h"
#include "MarketDataWidget.h"
#include "TabWithShortcut.h"
#include "UtxoReservationToken.h"

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
   class UTXOReservationManager;
}

namespace Blocksettle {
   namespace Communication {
      namespace ProxyTerminalPb {
         class Response;
         class Response_FutureResponse;
      }
   }
}

class ApplicationSettings;
class ArmoryConnection;
class AssetManager;
class AuthAddressManager;
class AutoSignScriptProvider;
class CelerClientQt;
class DialogManager;
class HeadlessContainer;
class MarketDataProvider;
class MDCallbacksQt;
class OrderListModel;
class QuoteProvider;
class RFQDialog;
class RfqStorage;

class RFQRequestWidget : public TabWithShortcut
{
Q_OBJECT

public:
   RFQRequestWidget(QWidget* parent = nullptr);
   ~RFQRequestWidget() override;

   void initWidgets(const std::shared_ptr<MarketDataProvider> &
      , const std::shared_ptr<MDCallbacksQt> &
      , const std::shared_ptr<ApplicationSettings> &);

   void init(const std::shared_ptr<spdlog::logger> &
      , const std::shared_ptr<CelerClientQt> &
      , const std::shared_ptr<AuthAddressManager> &
      , const std::shared_ptr<QuoteProvider> &
      , const std::shared_ptr<AssetManager> &
      , const std::shared_ptr<DialogManager> &
      , const std::shared_ptr<HeadlessContainer> &
      , const std::shared_ptr<ArmoryConnection> &
      , const std::shared_ptr<AutoSignScriptProvider> &
      , const std::shared_ptr<bs::UTXOReservationManager> &
      , OrderListModel *orderListModel);

   void setWalletsManager(const std::shared_ptr<bs::sync::WalletsManager> &);

   void shortcutActivated(ShortcutType s) override;

   void setAuthorized(bool authorized);

protected:
   void hideEvent(QHideEvent* event) override;
   bool eventFilter(QObject* sender, QEvent* event) override;

signals:
   void requestPrimaryWalletCreation();
   void loginRequested();

   void sendUnsignedPayinToPB(const std::string& settlementId, const bs::network::UnsignedPayinData& unsignedPayinData);
   void sendSignedPayinToPB(const std::string& settlementId, const BinaryData& signedPayin);
   void sendSignedPayoutToPB(const std::string& settlementId, const BinaryData& signedPayout);

   void sendFutureRequestToPB(const bs::network::FutureRequest &details);

   void cancelXBTTrade(const std::string& settlementId);
   void cancelCCTrade(const std::string& orderId);

   void unsignedPayinRequested(const std::string& settlementId);
   void signedPayoutRequested(const std::string& settlementId, const BinaryData& payinHash, QDateTime timestamp);
   void signedPayinRequested(const std::string& settlementId, const BinaryData& unsignedPayin
      , const BinaryData &payinHash, QDateTime timestamp);

   void CreateObligationDeliveryTX(const QModelIndex& index);

private:
   void showEditableRFQPage();
   void showFuturesPage(bs::network::Asset::Type type);
   void popShield();

   bool checkConditions(const MarketSelectedInfo& productGroup);
   bool checkWalletSettings(bs::network::Asset::Type productType
      , const MarketSelectedInfo& productGroup);
   void onRFQSubmit(const std::string &rfqId, const bs::network::RFQ& rfq
      , bs::UtxoReservationToken ccUtxoRes);
   void onRFQCancel(const std::string &rfqId);
   void deleteDialog(const std::string &rfqId);
   void processFutureResponse(const Blocksettle::Communication::ProxyTerminalPb::Response_FutureResponse &msg);

public slots:
   void onCurrencySelected(const MarketSelectedInfo& selectedInfo);
   void onBidClicked(const MarketSelectedInfo& selectedInfo);
   void onAskClicked(const MarketSelectedInfo& selectedInfo);
   void onDisableSelectedInfo();
   void onRefreshFocus();

   void onMessageFromPB(const Blocksettle::Communication::ProxyTerminalPb::Response &response);
   void onUserConnected(const bs::network::UserType &);
   void onUserDisconnected();

private slots:
   void onConnectedToCeler();
   void onDisconnectedFromCeler();
   void onRFQAccepted(const std::string &id);
   void onRFQExpired(const std::string &id);
   void onRFQCancelled(const std::string &id);

   void onOrderClicked(const QModelIndex &index);

public slots:
   void forceCheckCondition();

private:
   std::unique_ptr<Ui::RFQRequestWidget> ui_;

   std::shared_ptr<spdlog::logger>     logger_;
   std::shared_ptr<CelerClientQt>      celerClient_;
   std::shared_ptr<QuoteProvider>      quoteProvider_;
   std::shared_ptr<AssetManager>       assetManager_;
   std::shared_ptr<AuthAddressManager> authAddressManager_;
   std::shared_ptr<DialogManager>      dialogManager_;
   OrderListModel*                     orderListModel_;

   std::shared_ptr<bs::sync::WalletsManager> walletsManager_;
   std::shared_ptr<HeadlessContainer>  signingContainer_;
   std::shared_ptr<ArmoryConnection>   armory_;
   std::shared_ptr<ApplicationSettings> appSettings_;
   std::shared_ptr<AutoSignScriptProvider>      autoSignProvider_;
   std::shared_ptr<bs::UTXOReservationManager>  utxoReservationManager_;

   std::shared_ptr<RfqStorage> rfqStorage_;

   QList<QMetaObject::Connection>   marketDataConnection;

   std::unordered_map<std::string, RFQDialog *> dialogs_;
};

#endif // __RFQ_REQUEST_WIDGET_H__
