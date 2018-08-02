#ifndef __CC_SETTLEMENT_TRANSACTION_WIDGET_H__
#define __CC_SETTLEMENT_TRANSACTION_WIDGET_H__

#include <QWidget>
#include <QTimer>
#include <memory>
#include <atomic>

#include "BinaryData.h"
#include "CommonTypes.h"
#include "FrejaREST.h"
#include "MetaData.h"
#include "SettlementWallet.h"
#include "UtxoReservation.h"

namespace Ui {
    class CCSettlementTransactionWidget;
}
namespace spdlog {
   class logger;
}
class ArmoryConnection;
class AssetManager;
class SignContainer;
class TransactionData;
class WalletsManager;
class CelerClient;

namespace SwigClient
{
   class BtcWallet;
};

class CCSettlementTransactionWidget : public QWidget
{
Q_OBJECT

public:
   CCSettlementTransactionWidget(const std::shared_ptr<spdlog::logger> &
      , const std::shared_ptr<AssetManager> &
      , const std::shared_ptr<SignContainer> &
      , const std::shared_ptr<ArmoryConnection> &
      , const std::shared_ptr<CelerClient> &
      , QWidget* parent = nullptr);
   ~CCSettlementTransactionWidget() override;

   void reset(const std::shared_ptr<WalletsManager> &walletsManager);
   void populateDetails(const bs::network::RFQ& rfq, const bs::network::Quote& quote
      , const std::shared_ptr<TransactionData>& transactionData, const bs::Address &genesis);
   const std::string getCCTxData() const;
   const std::string getTxSignedData() const;

   Q_INVOKABLE void cancel();

private:
   void setupTimer();
   void onCancel();
   void onAccept();

   void populateCCDetails(const bs::network::RFQ& rfq, const bs::network::Quote& quote, const bs::Address &gen);

   void acceptSpotCC();
   bool createCCUnsignedTXdata();
   bool createCCSignedTXdata();
   void startFrejaSign();

private slots:
   void ticker();
   void onPasswordUpdated(const QString &);
   void updateAcceptButton();
   void onGenAddrVerified(bool);
   void onHDWalletInfo(unsigned int id, bs::wallet::EncryptionType, const SecureBinaryData &);
   void onTXSigned(unsigned int id, BinaryData signedTX, std::string error);

   void onFrejaSucceeded(SecureBinaryData);
   void onFrejaFailed(const QString &);
   void onFrejaStatusUpdated(const QString &);

signals:
   void settlementCancelled();
   void settlementAccepted();
   void sendOrder();
   void accepted();
   void genAddrVerified(bool result);

private:
   Ui::CCSettlementTransactionWidget* ui_;
   QTimer                     timer_;
   QDateTime                  expireTime_;
   bool                       clientSells_ = false;

   bs::network::RFQ           rfq_;
   bs::network::Quote         quote_;
   std::string                reserveId_;
   BinaryData                 dealerTx_;
   BinaryData                 requesterTx_;
   double                     amount_ = 0;
   double                     price_ = 0;
   bs::wallet::TXSignRequest  ccTxData_;
   std::string                ccTxSigned_;

   std::string                product_;
   std::string                dealerAddress_;

   QString                    sValid;
   QString                    sInvalid;
   bool                       userKeyOk_ = false;
   unsigned int               ccSignId_ = 0;
   unsigned int               infoReqId_ = 0;

   std::shared_ptr<spdlog::logger>     logger_;
   std::shared_ptr<AssetManager>       assetManager_;
   std::shared_ptr<TransactionData>    transactionData_;
   std::shared_ptr<WalletsManager>     walletsManager_;
   std::shared_ptr<SignContainer>      signingContainer_;
   std::shared_ptr<ArmoryConnection>   armory_;

   std::shared_ptr<bs::UtxoReservation::Adapter>   utxoAdapter_;

   std::shared_ptr<FrejaSignWallet> frejaSign_;
   bs::wallet::EncryptionType       encType_ = bs::wallet::EncryptionType::Unencrypted;
   QString           userId_;
   SecureBinaryData  walletPassword_;
};

#endif // __CC_SETTLEMENT_TRANSACTION_WIDGET_H__
