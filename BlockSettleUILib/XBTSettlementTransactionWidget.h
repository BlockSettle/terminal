#ifndef __XBT_SETTLEMENT_TRANSACTION_WIDGET_H__
#define __XBT_SETTLEMENT_TRANSACTION_WIDGET_H__

#include <QWidget>
#include <QTimer>

#include <memory>
#include <atomic>

#include "AddressVerificator.h"
#include "BinaryData.h"
#include "CommonTypes.h"
#include "MetaData.h"
#include "SettlementWallet.h"
#include "UtxoReservation.h"

namespace Ui {
    class XBTSettlementTransactionWidget;
}

namespace spdlog {
   class logger;
}
class ArmoryConnection;
class AssetManager;
class AuthAddressManager;
class SignContainer;
class QuoteProvider;
class TransactionData;
class WalletsManager;
class CelerClient;

namespace SwigClient
{
   class BtcWallet;
};

class XBTSettlementTransactionWidget : public QWidget
{
Q_OBJECT

public:
   XBTSettlementTransactionWidget(const std::shared_ptr<spdlog::logger> &, const std::shared_ptr<AuthAddressManager> &
      , const std::shared_ptr<AssetManager> &, const std::shared_ptr<QuoteProvider> &
      , const std::shared_ptr<SignContainer> &, const std::shared_ptr<ArmoryConnection> &
      , const std::shared_ptr<CelerClient> &, QWidget* parent = nullptr);
   ~XBTSettlementTransactionWidget() override;

   void reset(const std::shared_ptr<WalletsManager> &walletsManager);
   void populateDetails(const bs::network::RFQ& rfq, const bs::network::Quote& quote
      , const std::shared_ptr<TransactionData>& transactionData);

   void OrderReceived();

   Q_INVOKABLE void cancel();

private:
   void setupTimer();
   void onCancel();
   void onAccept();
   void payoutOnCancel();
   void detectDealerTxs();

   void populateXBTDetails(const bs::network::Quote& quote);

   unsigned int createPayoutTx(const BinaryData& payinHash, double qty, const bs::Address &recvAddr);

   void acceptSpotXBT();

private slots:
   void ticker();
   void stop();
   void retry();
   void updateAcceptButton();
   void onZCError(const QString &txHash, const QString &errMsg);
   void onPayInZCDetected();
   void onPayoutZCDetected(int confNum, bs::PayoutSigner::Type);

   void onHDWalletInfo(unsigned int id, std::vector<bs::wallet::EncryptionType>
      , std::vector<SecureBinaryData> encKeys, bs::wallet::KeyRank);
   void onTXSigned(unsigned int id, BinaryData signedTX, std::string error);
   void onDealerVerificationStateChanged();

signals:
   void settlementCancelled();
   void settlementAccepted();
   void accepted();

   void DealerVerificationStateChanged();

private:
   Ui::XBTSettlementTransactionWidget* ui_;
   QTimer                     timer_;
   QDateTime                  expireTime_;
   bool                       clientSells_ = false;

   bs::network::RFQ           rfq_;
   bs::network::Quote         quote_;
   BinaryData                 settlementId_;
   std::string                reserveId_;
   BinaryData                 userKey_;
   bs::Address                recvAddr_;
   BinaryData                 dealerTx_;
   BinaryData                 requesterTx_;
   double                     amount_ = 0;
   double                     price_ = 0;

   BinaryData                 payinData_;
   BinaryData                 payoutData_;
   std::string                product_;
   std::string                dealerAddress_;

   AddressVerificationState   dealerVerifState_ = AddressVerificationState::InProgress;
   QString                    sValid;
   QString                    sInvalid;
   QString                    sFailed;
   bool                       userKeyOk_ = false;
   std::atomic_bool           waitForPayout_;
   std::atomic_bool           waitForPayin_;
   unsigned int               payinSignId_ = 0;
   unsigned int               payoutSignId_ = 0;
   unsigned int               infoReqId_ = 0;
   unsigned int               infoReqIdAuth_ = 0;

   std::shared_ptr<spdlog::logger>        logger_;
   std::shared_ptr<AuthAddressManager>    authAddressManager_;
   std::shared_ptr<AssetManager>          assetManager_;
   std::shared_ptr<QuoteProvider>         quoteProvider_;
   std::shared_ptr<bs::SettlementAddressEntry>  settlAddr_;
   std::shared_ptr<TransactionData>       transactionData_;
   std::shared_ptr<WalletsManager>        walletsManager_;
   std::shared_ptr<AddressVerificator>    addrVerificator_;
   std::shared_ptr<SignContainer>         signingContainer_;
   std::shared_ptr<ArmoryConnection>      armory_;

   std::shared_ptr<bs::SettlementMonitor>          monitor_;
   std::shared_ptr<bs::UtxoReservation::Adapter>   utxoAdapter_;

   std::vector<bs::wallet::EncryptionType>   encTypes_, encTypesAuth_;
   std::vector<SecureBinaryData>             encKeys_, encKeysAuth_;
   bs::wallet::KeyRank                       keyRank_, keyRankAuth_;
   std::string comment_;

   bool sellFromPrimary_;
};

#endif // __XBT_SETTLEMENT_TRANSACTION_WIDGET_H__
