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
class AssetManager;
class AuthAddressManager;
class CCSettlementTransactionWidget;
class QuoteProvider;
class SignContainer;
class WalletsManager;
class XBTSettlementTransactionWidget;

class RFQDialog : public QDialog
{
Q_OBJECT

public:
   RFQDialog(const std::shared_ptr<spdlog::logger> &logger, const bs::network::RFQ& rfq
      , const std::shared_ptr<TransactionData>& transactionData
      , const std::shared_ptr<QuoteProvider>& quoteProvider
      , const std::shared_ptr<AuthAddressManager>& authAddressManager
      , const std::shared_ptr<AssetManager>& assetManager
      , const std::shared_ptr<WalletsManager> &walletsManager
      , const std::shared_ptr<SignContainer> &
      , QWidget* parent = nullptr);
   ~RFQDialog() override = default;

protected:
   void reject() override;
   bool close();

private slots:
   void onRFQResponseAccepted(const QString &reqId, const bs::network::Quote& quote);
   void onRFQCancelled(const QString &reqId);
   void onQuoteReceived(const bs::network::Quote& quote);
   void onOrderFilled(const std::string &quoteId);
   void onOrderUpdated(const bs::network::Order& order);
   void onOrderFailed(const std::string& quoteId, const std::string& reason);
   void onSettlementAccepted();
   void onSignTxRequested(QString orderId, QString reqId);
   void onSettlementOrder();

private:
   Ui::RFQDialog* ui_;
   std::shared_ptr<spdlog::logger>     logger_;
   const bs::network::RFQ              rfq_;
   bs::network::Quote                  quote_;
   std::shared_ptr<TransactionData>    transactionData_;
   std::shared_ptr<QuoteProvider>      quoteProvider_;
   std::shared_ptr<AuthAddressManager> authAddressManager_;
   std::shared_ptr<WalletsManager>     walletsManager_;
   std::shared_ptr<SignContainer>      container_;
   std::shared_ptr<AssetManager>       assetMgr_;
   std::unordered_map<std::string, std::string> ccTxMap_;
   std::map<QString, QString>          ccReqIdToOrder_;

   bs::network::Order                  XBTOrder_;

   XBTSettlementTransactionWidget      *xbtSettlementWidget_ = nullptr;
   CCSettlementTransactionWidget       *ccSettlementWidget_ = nullptr;

   bool  cancelOnClose_ = true;
};

#endif // __RFQ_DIALOG_H__
