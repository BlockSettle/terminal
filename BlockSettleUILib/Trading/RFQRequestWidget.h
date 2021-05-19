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
#include "Celer/BaseCelerClient.h"
#include "CommonTypes.h"
#include "MarketDataWidget.h"
#include "SignerDefs.h"
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
   struct TradeSettings;
   class UTXOReservationManager;
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
class AutoSignScriptProvider;
class CelerClientQt;
class DialogManager;
class MarketDataProvider;
class MDCallbacksQt;
class OrderListModel;
class QuoteProvider;
class RFQDialog;
class RfqStorage;
class WalletSignerContainer;

class RFQRequestWidget : public TabWithShortcut
{
Q_OBJECT

public:
   RFQRequestWidget(QWidget* parent = nullptr);
   ~RFQRequestWidget() override;

   void init(const std::shared_ptr<spdlog::logger>&
      , const std::shared_ptr<DialogManager>&
      , OrderListModel* orderListModel);

   void shortcutActivated(ShortcutType s) override;

   void setAuthorized(bool authorized);

   void onNewSecurity(const std::string& name, bs::network::Asset::Type);
   void onMDUpdated(bs::network::Asset::Type, const QString& security
      , const bs::network::MDFields &);
   void onBalance(const std::string& currency, double balance);
   void onWalletBalance(const bs::sync::WalletBalanceData&);
   void onHDWallet(const bs::sync::HDWalletData&);
   void onWalletData(const std::string& walletId, const bs::sync::WalletData&);

   void onMatchingLogin(const std::string& mtchLogin, BaseCelerClient::CelerUserType
      , const std::string& userId);
   void onMatchingLogout();
   void onVerifiedAuthAddresses(const std::vector<bs::Address>&);
   void onAuthKey(const bs::Address&, const BinaryData& authKey);
   void onTradeSettings(const std::shared_ptr<bs::TradeSettings>&);

   void onQuoteReceived(const bs::network::Quote&);
   void onQuoteMatched(const std::string &rfqId, const std::string& quoteId);
   void onQuoteFailed(const std::string& rfqId, const std::string& quoteId
      , const std::string& info);
   void onSettlementPending(const std::string& rfqId, const std::string& quoteId
      , const BinaryData& settlementId, int timeLeftMS);
   void onSettlementComplete(const std::string& rfqId, const std::string& quoteId
      , const BinaryData& settlementId);

   void onReservedUTXOs(const std::string& resId, const std::string &subId
      , const std::vector<UTXO>&);

protected:
   void hideEvent(QHideEvent* event) override;
   bool eventFilter(QObject* sender, QEvent* event) override;

signals:
   void requestPrimaryWalletCreation();
   void loginRequested();

   void sendUnsignedPayinToPB(const std::string& settlementId, const bs::network::UnsignedPayinData& unsignedPayinData);
   void sendSignedPayinToPB(const std::string& settlementId, const BinaryData& signedPayin);
   void sendSignedPayoutToPB(const std::string& settlementId, const BinaryData& signedPayout);

   void cancelXBTTrade(const std::string& settlementId);
   void cancelCCTrade(const std::string& orderId);

   void unsignedPayinRequested(const std::string& settlementId);
   void signedPayoutRequested(const std::string& settlementId, const BinaryData& payinHash, QDateTime timestamp);
   void signedPayinRequested(const std::string& settlementId, const BinaryData& unsignedPayin
      , const BinaryData &payinHash, QDateTime timestamp);

   void needWalletData(const std::string& walletId);
   void needSubmitRFQ(const bs::network::RFQ&, const std::string& reserveId = {});
   void needAcceptRFQ(const std::string& id, const bs::network::Quote&);
   void needExpireRFQ(const std::string& id);
   void needCancelRFQ(const std::string& id);
   void needAuthKey(const bs::Address&);

   void needReserveUTXOs(const std::string& reserveId, const std::string& subId
      , uint64_t amount, bool withZC = false, const std::vector<UTXO>& utxos = {});

private:
   void showEditableRFQPage();
   void popShield();

   bool checkConditions(const MarketSelectedInfo& productGroup);
   bool checkWalletSettings(bs::network::Asset::Type productType
      , const MarketSelectedInfo& productGroup);
   void onRFQSubmit(const std::string &rfqId, const bs::network::RFQ& rfq
      , bs::UtxoReservationToken ccUtxoRes);
   void onRFQCancel(const std::string &rfqId);
   void deleteDialog(const std::string &rfqId);

public slots:
   void onCurrencySelected(const MarketSelectedInfo& selectedInfo);
   void onBidClicked(const MarketSelectedInfo& selectedInfo);
   void onAskClicked(const MarketSelectedInfo& selectedInfo);
   void onDisableSelectedInfo();
   void onRefreshFocus();

   void onMessageFromPB(const Blocksettle::Communication::ProxyTerminalPb::Response &response); //deprecated
   void onUserConnected(const bs::network::UserType &);
   void onUserDisconnected();

private slots:
   void onConnectedToCeler();
   void onDisconnectedFromCeler();
   void onRFQAccepted(const std::string &id, const bs::network::Quote&);
   void onRFQExpired(const std::string &id);
   void onRFQCancelled(const std::string &id);

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

   std::shared_ptr<bs::sync::WalletsManager> walletsManager_;
   std::shared_ptr<WalletSignerContainer>    signingContainer_;
   std::shared_ptr<ArmoryConnection>   armory_;
   std::shared_ptr<ApplicationSettings> appSettings_;
   std::shared_ptr<AutoSignScriptProvider>      autoSignProvider_;
   std::shared_ptr<bs::UTXOReservationManager>  utxoReservationManager_;

   std::shared_ptr<RfqStorage> rfqStorage_;

   QList<QMetaObject::Connection>   marketDataConnection_;

   std::unordered_map<std::string, double>      balances_;
   std::unordered_map<std::string, RFQDialog *> dialogs_;

   BaseCelerClient::CelerUserType   userType_{ BaseCelerClient::CelerUserType::Undefined };
};

#endif // __RFQ_REQUEST_WIDGET_H__
