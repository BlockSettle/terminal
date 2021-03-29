/*

***********************************************************************************
* Copyright (C) 2019 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __RFQ_DIALOG_H__
#define __RFQ_DIALOG_H__

#include <QDialog>

#include <memory>

#include "CommonTypes.h"
#include "UtxoReservationToken.h"
#include "BSErrorCode.h"
#include "HDPath.h"

namespace Ui {
   class RFQDialog;
}
namespace spdlog {
   class logger;
}
namespace bs {
   namespace sync {
      namespace hd {
         class Wallet;
      }
      class Wallet;
      class WalletsManager;
   }
   class SettlementContainer;
   class UTXOReservationManager;
}
class ApplicationSettings;
class ArmoryConnection;
class AssetManager;
class AuthAddressManager;
class CelerClientQt;
class CCSettlementTransactionWidget;
class HeadlessContainer;
class QuoteProvider;
class RFQRequestWidget;
class ReqCCSettlementContainer;
class ReqXBTSettlementContainer;
class RfqStorage;
class XBTSettlementTransactionWidget;

class RFQDialog : public QDialog
{
Q_OBJECT

public:
   [[deprecated]] RFQDialog(const std::shared_ptr<spdlog::logger> &logger
      , const std::string &id, const bs::network::RFQ& rfq
      , const std::shared_ptr<QuoteProvider>& quoteProvider
      , const std::shared_ptr<AuthAddressManager>& authAddressManager
      , const std::shared_ptr<AssetManager>& assetManager
      , const std::shared_ptr<bs::sync::WalletsManager> &walletsManager
      , const std::shared_ptr<HeadlessContainer> &
      , const std::shared_ptr<ArmoryConnection> &
      , const std::shared_ptr<CelerClientQt> &celerClient
      , const std::shared_ptr<ApplicationSettings> &appSettings
      , const std::shared_ptr<RfqStorage> &rfqStorage
      , const std::shared_ptr<bs::sync::hd::Wallet> &xbtWallet
      , const bs::Address &recvXbtAddrIfSet
      , const bs::Address &authAddr
      , const std::shared_ptr<bs::UTXOReservationManager> &utxoReservationManager
      , const std::map<UTXO, std::string> &fixedXbtInputs
      , bs::UtxoReservationToken fixedXbtUtxoRes
      , bs::UtxoReservationToken ccUtxoRes
      , bs::hd::Purpose purpose
      , RFQRequestWidget* parent = nullptr);
   RFQDialog(const std::shared_ptr<spdlog::logger>& logger
      , const std::string& id, const bs::network::RFQ& rfq
      , const std::string& xbtWalletId, const bs::Address& recvXbtAddrIfSet
      , const bs::Address& authAddr
      , bs::hd::Purpose purpose
      , RFQRequestWidget* parent = nullptr);
   ~RFQDialog() override;

   void cancel(bool force = true);

   void onBalance(const std::string& currency, double balance);
   void onMatchingLogout();
   void onSettlementPending(const std::string& quoteId, const BinaryData& settlementId);
   void onSettlementComplete();

signals:
   void accepted(const std::string &id, const bs::network::Quote&);
   void expired(const std::string &id);
   void cancelled(const std::string &id);

protected:
   void reject() override;

public slots:
   void onUnsignedPayinRequested(const std::string& settlementId);
   void onSignedPayoutRequested(const std::string& settlementId, const BinaryData& payinHash, QDateTime timestamp);
   void onSignedPayinRequested(const std::string& settlementId, const BinaryData& unsignedPayin
      , const BinaryData &payinHash, QDateTime timestamp);
   void onQuoteReceived(const bs::network::Quote& quote);
   void onOrderFilled(const std::string& quoteId);
   void onOrderFailed(const std::string& quoteId, const std::string& reason);

private slots:
   bool close();
   void onTimeout();
   void onQuoteFinished();
   void onQuoteFailed();

   void onRFQResponseAccepted(const std::string &reqId, const bs::network::Quote& quote);
   void onXBTSettlementAccepted();

   void onSignTxRequested(QString orderId, QString reqId, QDateTime timestamp);
   void onCCQuoteAccepted();
   void onCCTxSigned();

   void onXBTQuoteAccept(std::string reqId, std::string hexPayoutTx);
   void logError(const std::string &id, bs::error::ErrorCode code
      , const QString &errorMessage);

private:
   std::shared_ptr<bs::SettlementContainer> newCCcontainer();
   std::shared_ptr<bs::SettlementContainer> newXBTcontainer();
   void hideIfNoRemoteSignerMode();

private:
   std::unique_ptr<Ui::RFQDialog> ui_;
   std::shared_ptr<spdlog::logger>     logger_;
   const std::string                   id_;
   const bs::network::RFQ              rfq_;
   bs::network::Quote                  quote_;
   bs::Address recvXbtAddrIfSet_;

   std::shared_ptr<QuoteProvider>               quoteProvider_;
   std::shared_ptr<AuthAddressManager>          authAddressManager_;
   std::shared_ptr<bs::sync::WalletsManager>    walletsManager_;
   std::shared_ptr<HeadlessContainer>           signContainer_;
   std::shared_ptr<AssetManager>                assetMgr_;
   std::shared_ptr<ArmoryConnection>            armory_;
   std::shared_ptr<CelerClientQt>               celerClient_;
   std::shared_ptr<ApplicationSettings>         appSettings_;
   std::shared_ptr<RfqStorage>                  rfqStorage_;
   std::shared_ptr<bs::sync::hd::Wallet>        xbtWallet_;
   std::shared_ptr<bs::UTXOReservationManager> utxoReservationManager_;

   std::shared_ptr<bs::SettlementContainer>     curContainer_;
   std::shared_ptr<ReqCCSettlementContainer>    ccSettlContainer_;
   std::shared_ptr<ReqXBTSettlementContainer>   xbtSettlContainer_;

   const bs::Address authAddr_;
   const std::map<UTXO, std::string>   fixedXbtInputs_;
   bs::UtxoReservationToken fixedXbtUtxoRes_;

   bool  cancelOnClose_ = true;
   bool isRejectStarted_ = false;

   RFQRequestWidget *requestWidget_{};

   QString                    ccOrderId_;
   bs::UtxoReservationToken   ccUtxoRes_;
   bs::hd::Purpose            walletPurpose_;

};

#endif // __RFQ_DIALOG_H__
