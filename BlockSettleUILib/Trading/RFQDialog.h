#ifndef __RFQ_DIALOG_H__
#define __RFQ_DIALOG_H__

#include <QDialog>

#include <memory>

#include "CommonTypes.h"
#include "TransactionData.h"

namespace Ui {
   class RFQDialog;
}
namespace spdlog {
   class logger;
}
namespace bs {
   namespace sync {
      class WalletsManager;
   }
   class SettlementContainer;
}
class ArmoryConnection;
class AssetManager;
class AuthAddressManager;
class CCSettlementTransactionWidget;
class QuoteProvider;
class ReqCCSettlementContainer;
class ReqXBTSettlementContainer;
class SignContainer;
class XBTSettlementTransactionWidget;
class BaseCelerClient;
class ApplicationSettings;
class ConnectionManager;

class RFQDialog : public QDialog
{
Q_OBJECT

public:
   RFQDialog(const std::shared_ptr<spdlog::logger> &logger, const bs::network::RFQ& rfq
      , const std::shared_ptr<TransactionData>& transactionData
      , const std::shared_ptr<QuoteProvider>& quoteProvider
      , const std::shared_ptr<AuthAddressManager>& authAddressManager
      , const std::shared_ptr<AssetManager>& assetManager
      , const std::shared_ptr<bs::sync::WalletsManager> &walletsManager
      , const std::shared_ptr<SignContainer> &
      , const std::shared_ptr<ArmoryConnection> &
      , const std::shared_ptr<BaseCelerClient> &celerClient
      , const std::shared_ptr<ApplicationSettings> &appSettings
      , const std::shared_ptr<ConnectionManager> &
      , const bs::Address &authAddr
      , QWidget* parent = nullptr);
   ~RFQDialog() override;

protected:
   void reject() override;

private slots:
   bool close();

   void onRFQResponseAccepted(const QString &reqId, const bs::network::Quote& quote);
   void onQuoteReceived(const bs::network::Quote& quote);
   void onOrderFilled(const std::string &quoteId);
   void onOrderUpdated(const bs::network::Order& order);
   void onOrderFailed(const std::string& quoteId, const std::string& reason);
   void onSettlementAccepted();
   void onSignTxRequested(QString orderId, QString reqId);
   void onSettlementOrder();
   void onXBTQuoteAccept(std::string reqId, std::string hexPayoutTx);

private:
   std::shared_ptr<bs::SettlementContainer> newCCcontainer();
   std::shared_ptr<bs::SettlementContainer> newXBTcontainer();

private:
   std::unique_ptr<Ui::RFQDialog> ui_;
   std::shared_ptr<spdlog::logger>     logger_;
   const bs::network::RFQ              rfq_;
   bs::network::Quote                  quote_;
   std::shared_ptr<TransactionData>    transactionData_;
   std::shared_ptr<QuoteProvider>      quoteProvider_;
   std::shared_ptr<AuthAddressManager> authAddressManager_;
   std::shared_ptr<bs::sync::WalletsManager> walletsManager_;
   std::shared_ptr<SignContainer>      signContainer_;
   std::shared_ptr<AssetManager>       assetMgr_;
   std::shared_ptr<ArmoryConnection>   armory_;
   std::shared_ptr<BaseCelerClient>        celerClient_;
   std::shared_ptr<ApplicationSettings> appSettings_;
   std::shared_ptr<ConnectionManager>  connectionManager_;
   std::unordered_map<std::string, std::string> ccTxMap_;
   std::map<QString, QString>          ccReqIdToOrder_;

   bs::network::Order                  XBTOrder_;

   std::shared_ptr<bs::SettlementContainer>     curContainer_;
   std::shared_ptr<ReqCCSettlementContainer>    ccSettlContainer_;
   std::shared_ptr<ReqXBTSettlementContainer>   xbtSettlContainer_;
   const bs::Address authAddr_;

   bool  cancelOnClose_ = true;
   bool isRejectStarted_ = false;
};

#endif // __RFQ_DIALOG_H__
