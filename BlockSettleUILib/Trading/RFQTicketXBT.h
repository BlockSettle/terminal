/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
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

QT_BEGIN_NAMESPACE
class QPushButton;
class QLineEdit;
QT_END_NAMESPACE

namespace spdlog {
   class logger;
}
namespace Ui {
    class RFQTicketXBT;
}
namespace bs {
   namespace sync {
      namespace hd {
         class Leaf;
         class Wallet;
      }
      class Wallet;
      class WalletsManager;
   }
}
class ArmoryConnection;
class AssetManager;
class AuthAddressManager;
class CCAmountValidator;
class FXAmountValidator;
class QuoteProvider;
class SelectedTransactionInputs;
class SignContainer;
class TransactionData;
class XbtAmountValidator;


class RFQTicketXBT : public QWidget
{
Q_OBJECT

public:
   RFQTicketXBT(QWidget* parent = nullptr);
   ~RFQTicketXBT() override;

   void init(const std::shared_ptr<spdlog::logger> &logger
      , const std::shared_ptr<AuthAddressManager> &
      , const std::shared_ptr<AssetManager> &assetManager
      , const std::shared_ptr<QuoteProvider> &quoteProvider
      , const std::shared_ptr<SignContainer> &
      , const std::shared_ptr<ArmoryConnection> &);
   void setWalletsManager(const std::shared_ptr<bs::sync::WalletsManager> &);

   void resetTicket();

   std::map<UTXO, std::string> fixedXbtInputs() const;

   QPushButton* submitButton() const;
   QLineEdit* lineEditAmount() const;
   QPushButton* buyButton() const;
   QPushButton* sellButton() const;
   QPushButton* numCcyButton() const;
   QPushButton* denomCcyButton() const;

   bs::Address selectedAuthAddress() const;
   bs::Address recvXbtAddress() const;

   using SubmitRFQCb = std::function<void(const bs::network::RFQ& rfq, bs::UtxoReservationToken utxoRes)>;
   void setSubmitRFQ(SubmitRFQCb submitRFQCb);

   std::shared_ptr<bs::sync::hd::Wallet> xbtWallet() const;

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

private slots:
   void updateBalances();
   void onSignerReady();
   void walletsLoaded();

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

   void updatePanel();

   void fillRecvAddresses();

   BalanceInfoContainer getBalanceInfo() const;
   QString getProduct() const;
   std::shared_ptr<bs::sync::Wallet> getCCWallet(const std::string &cc) const;
   bool isXBTProduct() const;
   bool checkBalance(double qty) const;
   bs::network::Side::Type getSelectedSide() const;
   std::string authKey() const { return authKey_; }

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

   void SetCurrentIndicativePrices(const QString& bidPrice, const QString& offerPrice);
   void updateIndicativePrice();

   void productSelectionChanged();

   std::shared_ptr<bs::sync::hd::Wallet> getSendXbtWallet() const;
   std::shared_ptr<bs::sync::hd::Wallet> getRecvXbtWallet() const;
   bs::XBTAmount getXbtBalance() const;
   QString getProductToSpend() const;
   QString getProductToRecv() const;

private:
   std::unique_ptr<Ui::RFQTicketXBT> ui_;

   std::shared_ptr<spdlog::logger>     logger_;
   std::shared_ptr<AssetManager>       assetManager_;
   std::shared_ptr<AuthAddressManager> authAddressManager_;

   std::shared_ptr<bs::sync::WalletsManager> walletsManager_;
   std::shared_ptr<SignContainer>      signingContainer_;
   std::shared_ptr<ArmoryConnection>   armory_;

   bs::Address authAddr_;
   std::string authKey_;

   unsigned int      leafCreateReqId_ = 0;

   std::unordered_map<std::string, double>      rfqMap_;

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

   SubmitRFQCb submitRFQCb_;

   std::shared_ptr<SelectedTransactionInputs> selectedXbtInputs_;
};

#endif // __RFQ_TICKET_XBT_H__
