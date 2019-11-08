#ifndef __RFQ_DIALOG_H__
#define __RFQ_DIALOG_H__

#include <QDialog>

#include <memory>

#include "CommonTypes.h"
#include "UtxoReservationToken.h"

namespace Ui {
   class RFQDialog;
}
namespace spdlog {
   class logger;
}
namespace bs {
   namespace sync {
      class Wallet;
      class WalletsManager;
   }
   class SettlementContainer;
}
class ApplicationSettings;
class ArmoryConnection;
class AssetManager;
class AuthAddressManager;
class BaseCelerClient;
class CCSettlementTransactionWidget;
class ConnectionManager;
class QuoteProvider;
class RFQRequestWidget;
class ReqCCSettlementContainer;
class ReqXBTSettlementContainer;
class RfqStorage;
class SignContainer;
class XBTSettlementTransactionWidget;

class RFQDialog : public QDialog
{
Q_OBJECT

public:
   RFQDialog(const std::shared_ptr<spdlog::logger> &logger
      , const bs::network::RFQ& rfq
      , const std::shared_ptr<QuoteProvider>& quoteProvider
      , const std::shared_ptr<AuthAddressManager>& authAddressManager
      , const std::shared_ptr<AssetManager>& assetManager
      , const std::shared_ptr<bs::sync::WalletsManager> &walletsManager
      , const std::shared_ptr<SignContainer> &
      , const std::shared_ptr<ArmoryConnection> &
      , const std::shared_ptr<BaseCelerClient> &celerClient
      , const std::shared_ptr<ApplicationSettings> &appSettings
      , const std::shared_ptr<ConnectionManager> &
      , const std::shared_ptr<RfqStorage> &rfqStorage
      , const std::shared_ptr<bs::sync::Wallet> &xbtWallet
      , const bs::Address &recvXbtAddr
      , const bs::Address &authAddr
      , const std::vector<UTXO> &fixedXbtInputs
      , bs::UtxoReservationToken utxoRes
      , RFQRequestWidget* parent = nullptr);
   ~RFQDialog() override;

protected:
   void reject() override;

public slots:
   void onUnsignedPayinRequested(const std::string& settlementId);
   void onSignedPayoutRequested(const std::string& settlementId, const BinaryData& payinHash);
   void onSignedPayinRequested(const std::string& settlementId, const BinaryData& unsignedPayin);

private slots:
   bool close();

   void onRFQResponseAccepted(const QString &reqId, const bs::network::Quote& quote);
   void onQuoteReceived(const bs::network::Quote& quote);
   void onOrderFilled(const std::string &quoteId);
   void onOrderFailed(const std::string& quoteId, const std::string& reason);
   void onXBTSettlementAccepted();

   void onSignTxRequested(QString orderId, QString reqId);
   void onCCQuoteAccepted();
   void onCCTxSigned();

   void onXBTQuoteAccept(std::string reqId, std::string hexPayoutTx);
   void logError(const QString& errorMessage);

private:
   std::shared_ptr<bs::SettlementContainer> newCCcontainer();
   std::shared_ptr<bs::SettlementContainer> newXBTcontainer();

private:
   std::unique_ptr<Ui::RFQDialog> ui_;
   std::shared_ptr<spdlog::logger>     logger_;
   const bs::network::RFQ              rfq_;
   bs::network::Quote                  quote_;
   bs::Address recvXbtAddr_;

   std::shared_ptr<QuoteProvider>               quoteProvider_;
   std::shared_ptr<AuthAddressManager>          authAddressManager_;
   std::shared_ptr<bs::sync::WalletsManager>    walletsManager_;
   std::shared_ptr<SignContainer>               signContainer_;
   std::shared_ptr<AssetManager>                assetMgr_;
   std::shared_ptr<ArmoryConnection>            armory_;
   std::shared_ptr<BaseCelerClient>             celerClient_;
   std::shared_ptr<ApplicationSettings>         appSettings_;
   std::shared_ptr<ConnectionManager>           connectionManager_;
   std::shared_ptr<RfqStorage>                  rfqStorage_;
   std::shared_ptr<bs::sync::Wallet>            xbtWallet_;

   std::shared_ptr<bs::SettlementContainer>     curContainer_;
   std::shared_ptr<ReqCCSettlementContainer>    ccSettlContainer_;
   std::shared_ptr<ReqXBTSettlementContainer>   xbtSettlContainer_;

   const bs::Address authAddr_;
   const std::vector<UTXO> fixedXbtInputs_;

   bool  cancelOnClose_ = true;
   bool isRejectStarted_ = false;

   RFQRequestWidget *requestWidget_{};

   QString           ccOrderId_;
   bs::UtxoReservationToken utxoRes_;

};

#endif // __RFQ_DIALOG_H__
