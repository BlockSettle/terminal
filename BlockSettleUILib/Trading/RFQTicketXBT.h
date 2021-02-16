/*

***********************************************************************************
* Copyright (C) 2019 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __RFQ_TICKET_XBT_H__
#define __RFQ_TICKET_XBT_H__

#include <QFont>
#include <QWidget>

#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>

#include "BSErrorCode.h"
#include "CommonTypes.h"
#include "UtxoReservationToken.h"
#include "XBTAmount.h"
#include "UiUtils.h"

QT_BEGIN_NAMESPACE
class QPushButton;
class QLineEdit;
QT_END_NAMESPACE

namespace bs {
   struct TradeSettings;
}
namespace spdlog {
   class logger;
}
namespace Ui {
    class RFQTicketXBT;
}
class CCAmountValidator;
class FXAmountValidator;
class SelectedTransactionInputs;
class XbtAmountValidator;


class RFQTicketXBT : public QWidget
{
Q_OBJECT

public:
   RFQTicketXBT(QWidget* parent = nullptr);
   ~RFQTicketXBT() override;

   void init(const std::shared_ptr<spdlog::logger>&);

   void resetTicket();

   bs::FixedXbtInputs fixedXbtInputs();

   QPushButton* submitButton() const;
   QLineEdit* lineEditAmount() const;
   QPushButton* buyButton() const;
   QPushButton* sellButton() const;
   QPushButton* numCcyButton() const;
   QPushButton* denomCcyButton() const;

   bs::Address selectedAuthAddress() const;
   // returns empty address if automatic selected
   bs::Address recvXbtAddressIfSet() const;

   using SubmitRFQCb = std::function<void(const std::string &id
      , const bs::network::RFQ& rfq, bs::UtxoReservationToken ccUtxoRes)>;
   void setSubmitRFQ(SubmitRFQCb);
   using CancelRFQCb = std::function<void(const std::string &id)>;
   void setCancelRFQ(CancelRFQCb);

   UiUtils::WalletsTypes xbtWalletType() const;

   void onParentAboutToHide();

   void onVerifiedAuthAddresses(const std::vector<bs::Address>&);
   void onBalance(const std::string& currency, double balance);
   void onWalletBalance(const bs::sync::WalletBalanceData&);
   void onHDWallet(const bs::sync::HDWalletData&);
   void onWalletData(const std::string& walletId, const bs::sync::WalletData&);
   void onAuthKey(const bs::Address&, const BinaryData& authKey);
   void onTradeSettings(const std::shared_ptr<bs::TradeSettings>&);
   void onReservedUTXOs(const std::string& resId, const std::string& subId
      , const std::vector<UTXO>&);

signals:
   void needWalletData(const std::string& walletId);
   void needAuthKey(const bs::Address&);
   void needReserveUTXOs(const std::string& reserveId, const std::string& walletId
      , uint64_t amount, bool withZC = false, const std::vector<UTXO>& utxos = {});

public slots:
   void SetProductAndSide(const QString& productGroup, const QString& currencyPair
      , const QString& bidPrice, const QString& offerPrice, bs::network::Side::Type side);
   void setSecurityId(const QString& productGroup, const QString& currencyPair
      , const QString& bidPrice, const QString& offerPrice);
   void setSecurityBuy(const QString& productGroup, const QString& currencyPair
      , const QString& bidPrice, const QString& offerPrice);
   void setSecuritySell(const QString& productGroup, const QString& currencyPair
      , const QString& bidPrice, const QString& offerPrice);

   void enablePanel();
   void disablePanel();

   void onSendRFQ(const std::string &id, const QString &symbol, double amount, bool buy);
   void onCancelRFQ(const std::string &id);

   void onNewSecurity(const std::string& name, bs::network::Asset::Type at) { assetTypes_[name] = at; }
   void onMDUpdate(bs::network::Asset::Type, const QString &security, bs::network::MDFields);

private slots:
   void updateBalances();

   void onNumCcySelected();
   void onDenomCcySelected();

   void onSellSelected();
   void onBuySelected();

   void showCoinControl();
   void walletSelectedRecv(int index);
   void walletSelectedSend(int index);

   void updateSubmitButton();
   void submitButtonClicked();

   void onHDLeafCreated(const std::string& ccName);
   void onCreateHDWalletError(const std::string& ccName, bs::error::ErrorCode result);

   void onMaxClicked();
   void onAmountEdited(const QString &);

   void onCreateWalletClicked();

   void onAuthAddrChanged(int);

   void onUTXOReservationChanged(const std::string& walletId);

protected:
   bool eventFilter(QObject *watched, QEvent *evt) override;

private:
   enum class ProductGroupType
   {
      GroupNotSelected,
      FXGroupType,
      XBTGroupType,
      CCGroupType
   };

   struct BalanceInfoContainer
   {
      double            amount;
      QString           product;
      ProductGroupType  productType;
   };

private:
   void showHelp(const QString& helpText);
   void clearHelp();
   void sendRFQ(const std::string &id);

   void updatePanel();

   void fillRecvAddresses();

   bool preSubmitCheck();

   BalanceInfoContainer getBalanceInfo() const;
   QString getProduct() const;
   bool checkBalance(double qty) const;
   bool checkAuthAddr() const;
   bs::network::Side::Type getSelectedSide() const;
   std::string authKey() const;

   void putRFQ(const bs::network::RFQ &);
   bool existsRFQ(const bs::network::RFQ &);

   static std::string mkRFQkey(const bs::network::RFQ &);

   void SetProductGroup(const QString& productGroup);
   void SetCurrencyPair(const QString& currencyPair);

   void saveLastSideSelection(const std::string& product, const std::string& currencyPair, bs::network::Side::Type side);
   bs::network::Side::Type getLastSideSelection(const std::string& product, const std::string& currencyPair);

   void HideRFQControls();

   void initProductGroupMap();
   ProductGroupType getProductGroupType(const QString& productGroup);

   double getQuantity() const;
   double getOfferPrice() const;

   void SetCurrentIndicativePrices(const QString& bidPrice, const QString& offerPrice);
   void updateIndicativePrice();
   double getIndicativePrice() const;

   void productSelectionChanged();

   std::string getXbtLeafId(const std::string& hdWalletId) const;
   bool hasSendXbtWallet() const;
   bool hasRecvXbtWallet() const;
   bool hasCCWallet() const { return true; } //TODO: add proper checking
   bs::XBTAmount getXbtBalance() const;
   QString getProductToSpend() const;
   QString getProductToRecv() const;
   //bs::XBTAmount expectedXbtAmountMin() const;
   bs::XBTAmount getXbtReservationAmountForCc(double quantity, double offerPrice) const;

   void reserveBestUtxoSetAndSubmit(const std::string &id
      , const std::shared_ptr<bs::network::RFQ>& rfq);

   void sendDeferredRFQs();

private:
   std::unique_ptr<Ui::RFQTicketXBT> ui_;

   std::shared_ptr<spdlog::logger>     logger_;

   mutable bs::Address authAddr_;
   mutable std::string authKey_;

   unsigned int      leafCreateReqId_ = 0;

   std::unordered_map<std::string, double>      rfqMap_;
   std::unordered_map<std::string, std::shared_ptr<bs::network::RFQ>>   pendingRFQs_;

   std::unordered_map<std::string, bs::network::Side::Type>         lastSideSelection_;

   QFont    invalidBalanceFont_;

   CCAmountValidator                            *ccAmountValidator_{};
   FXAmountValidator                            *fxAmountValidator_{};
   XbtAmountValidator                           *xbtAmountValidator_{};

   std::unordered_map<std::string, ProductGroupType> groupNameToType_;
   ProductGroupType     currentGroupType_ = ProductGroupType::GroupNotSelected;

   QString currentProduct_;
   QString contraProduct_;

   QString currentBidPrice_;
   QString currentOfferPrice_;

   SubmitRFQCb submitRFQCb_{};
   CancelRFQCb cancelRFQCb_{};

   bs::FixedXbtInputs fixedXbtInputs_;

   bool  autoRFQenabled_{ false };
   std::vector<std::string>   deferredRFQs_;

   std::unordered_map<std::string, bs::network::Asset::Type>   assetTypes_;
   std::unordered_map<std::string, bs::network::MDInfo>  mdInfo_;
   std::unordered_map<std::string, double>   balances_;
   std::vector<bs::sync::HDWalletData>       wallets_;
   std::shared_ptr<bs::TradeSettings>        tradeSettings_;
   std::set<bs::Address>   authAddrs_;
};

#endif // __RFQ_TICKET_XBT_H__
