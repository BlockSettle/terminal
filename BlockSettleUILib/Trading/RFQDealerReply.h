/*

***********************************************************************************
* Copyright (C) 2019 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __RFQ_DEALER_REPLY_H__
#define __RFQ_DEALER_REPLY_H__

#include <QTimer>
#include <QWidget>

#include <atomic>
#include <functional>
#include <memory>
#include <unordered_map>
#include <unordered_set>

#include "BSErrorCode.h"
#include "CommonTypes.h"
#include "EncryptionUtils.h"
#include "QWalletInfo.h"
#include "HDPath.h"
#include "UtxoReservationToken.h"
#include "CommonTypes.h"

namespace Ui {
    class RFQDealerReply;
}
namespace spdlog {
   class logger;
}
namespace bs {
   namespace sync {
      namespace hd {
         class Leaf;
      }
      class Wallet;
      class WalletsManager;
   }
   class UTXOReservationManager;
}
class ApplicationSettings;
class ArmoryConnection;
class AssetManager;
class AuthAddressManager;
class AutoSignScriptProvider;
class QuoteProvider;
class SelectedTransactionInputs;
class SignContainer;
class CustomDoubleSpinBox;
class MarketDataProvider;
class ConnectionManager;

QT_BEGIN_NAMESPACE
class QDoubleSpinBox;
class QPushButton;
QT_END_NAMESPACE

namespace UiUtils {
   enum WalletsTypes : int;
}

namespace bs {
   namespace network {
      struct QuoteNotification;
   }

   namespace ui {

      struct SubmitQuoteReplyData
      {
         bs::network::QuoteNotification qn;
         bs::UtxoReservationToken utxoRes;
         std::shared_ptr<bs::sync::hd::Wallet> xbtWallet;
         std::string xbtWalletId;
         bs::Address authAddr;
         std::vector<UTXO> fixedXbtInputs;
         bs::hd::Purpose   walletPurpose;
         double   price;
      };

      class RFQDealerReply : public QWidget
      {
         Q_OBJECT

      public:
         RFQDealerReply(QWidget* parent = nullptr);
         ~RFQDealerReply() override;

         [[deprecated]] void init(const std::shared_ptr<spdlog::logger> logger
            , const std::shared_ptr<AuthAddressManager> &
            , const std::shared_ptr<AssetManager>& assetManager
            , const std::shared_ptr<QuoteProvider>& quoteProvider
            , const std::shared_ptr<ApplicationSettings> &
            , const std::shared_ptr<ConnectionManager> &connectionManager
            , const std::shared_ptr<SignContainer> &
            , const std::shared_ptr<ArmoryConnection> &
            , const std::shared_ptr<AutoSignScriptProvider> &autoSignQuoteProvider
            , const std::shared_ptr<bs::UTXOReservationManager>& utxoReservationManager);
         void init(const std::shared_ptr<spdlog::logger>&);

         [[deprecated]] void setWalletsManager(const std::shared_ptr<bs::sync::WalletsManager> &);

         CustomDoubleSpinBox* bidSpinBox() const;
         CustomDoubleSpinBox* offerSpinBox() const;

         QPushButton* pullButton() const;
         QPushButton* quoteButton() const;

         using SubmitQuoteNotifCb = std::function<void(const std::shared_ptr<SubmitQuoteReplyData> &data)>;
         void setSubmitQuoteNotifCb(SubmitQuoteNotifCb cb);

         using ResetCurrentReservationCb = std::function<void(const std::shared_ptr<SubmitQuoteReplyData> &data)>;
         void setResetCurrentReservation(ResetCurrentReservationCb cb);

         using GetLastUTXOReplyCb = std::function<const std::vector<UTXO>*(const std::string&)>;
         void setGetLastSettlementReply(GetLastUTXOReplyCb cb);

         void onParentAboutToHide();

         void onHDWallet(const bs::sync::HDWalletData&);
         void onBalance(const std::string& currency, double balance);
         void onWalletBalance(const bs::sync::WalletBalanceData&);
         void onAuthKey(const bs::Address&, const BinaryData& authKey);
         void onVerifiedAuthAddresses(const std::vector<bs::Address>&);
         void onReservedUTXOs(const std::string& resId, const std::string& subId
            , const std::vector<UTXO>&);

      signals:
         void pullQuoteNotif(const std::string& settlementId, const std::string& reqId, const std::string& reqSessToken);
         void needAuthKey(const bs::Address&);
         void needReserveUTXOs(const std::string& reserveId, const std::string& subId
            , uint64_t amount, bool partial = false, const std::vector<UTXO>& utxos = {});

      public slots:
         void setQuoteReqNotification(const network::QuoteReqNotification &, double indicBid, double indicAsk);
         void quoteReqNotifStatusChanged(const network::QuoteReqNotification &);
         void onMDUpdate(bs::network::Asset::Type, const QString &security, bs::network::MDFields);
         void onBestQuotePrice(const QString reqId, double price, bool own);
         void onCelerConnected();
         void onCelerDisconnected();
         void onAutoSignStateChanged();
         void onQuoteCancelled(const std::string &quoteId);

      private slots:
         void initUi();
         void priceChanged();
         void updateSubmitButton();
         void submitButtonClicked();
         void pullButtonClicked();
         void showCoinControl();
         void walletSelected(int index);
         void onTransactionDataChanged();
         void onAQReply(const bs::network::QuoteReqNotification &qrn, double price);
         void onReservedUtxosChanged(const std::string &walletId, const std::vector<UTXO> &);
         void onHDLeafCreated(const std::string& ccName);
         void onCreateHDWalletError(const std::string& ccName, bs::error::ErrorCode result);
         void onAuthAddrChanged(int);
         void onUTXOReservationChanged(const std::string& walletId);

      protected:
         bool eventFilter(QObject *watched, QEvent *evt) override;

      private:
         std::unique_ptr<Ui::RFQDealerReply> ui_;
         std::shared_ptr<spdlog::logger>        logger_;
         std::shared_ptr<bs::sync::WalletsManager> walletsManager_;
         std::shared_ptr<AuthAddressManager>    authAddressManager_;
         std::shared_ptr<AssetManager>          assetManager_;
         std::shared_ptr<QuoteProvider>         quoteProvider_;
         std::shared_ptr<ApplicationSettings>   appSettings_;
         std::shared_ptr<ConnectionManager>     connectionManager_;
         std::shared_ptr<SignContainer>         signingContainer_;
         std::shared_ptr<ArmoryConnection>      armory_;
         std::shared_ptr<AutoSignScriptProvider>   autoSignProvider_;
         std::shared_ptr<bs::UTXOReservationManager> utxoReservationManager_;
         std::string authKey_;
         bs::Address authAddr_;

         std::unordered_map<std::string, double>   sentNotifs_;
         network::QuoteReqNotification    currentQRN_;
         unsigned int   payInRecipId_{UINT_MAX};
         bool dealerSellXBT_{false};

         double   indicBid_{};
         double   indicAsk_{};
         std::atomic_bool     autoUpdatePrices_{true};

         std::string          autoSignWalletId_;

         std::string product_;
         std::string baseProduct_;

         bool           celerConnected_{false};

         std::unordered_map<std::string, double>   bestQPrices_;
         QFont invalidBalanceFont_;

         std::unordered_map<std::string, bs::network::MDInfo>  mdInfo_;

         std::vector<UTXO> selectedXbtInputs_;
         bs::UtxoReservationToken selectedXbtRes_;

         SubmitQuoteNotifCb submitQuoteNotifCb_;
         ResetCurrentReservationCb resetCurrentReservationCb_;
         GetLastUTXOReplyCb getLastUTXOReplyCb_;

         std::set<std::string> preparingCCRequest_;

      private:
         enum class ReplyType
         {
            Manual,
            Script,  //obsoleted, as we won't support scripting in the GUI
         };

         enum class AddressType
         {
            Recv,
            Change,

            Max = Change,
         };

         void reset();
         void validateGUI();
         void updateRespQuantity();
         void updateSpinboxes();
         void updateQuoteReqNotification(const network::QuoteReqNotification &);
         void updateBalanceLabel();
         double getPrice() const;
         double getValue() const;
         double getAmount() const;
         [[deprecated]] std::shared_ptr<bs::sync::Wallet> getCCWallet(const std::string &cc) const;
         [[deprecated]] std::shared_ptr<bs::sync::Wallet> getCCWallet(const bs::network::QuoteReqNotification &qrn) const;
         [[deprecated]] void getAddress(const std::string &quoteRequestId, const std::shared_ptr<bs::sync::Wallet> &wallet
            , AddressType type, std::function<void(bs::Address)> cb);
         void setBalanceOk(bool ok);
         bool checkBalance() const;
         XBTAmount getXbtBalance() const;
         BTCNumericTypes::balance_type getPrivateMarketCoinBalance() const;
         QDoubleSpinBox *getActivePriceWidget() const;
         void updateUiWalletFor(const bs::network::QuoteReqNotification &qrn);
         // xbtWallet - what XBT wallet to use for XBT/CC trades (selected from UI for manual trades, default wallet for AQ trades), empty for FX trades
         void submitReply(const network::QuoteReqNotification &qrn, double price, ReplyType replyType);
         void updateWalletsList(int walletsFlags);
         bool isXbtSpend() const;
         std::string getSelectedXbtWalletId(ReplyType replyType) const;
         [[deprecated]] std::shared_ptr<bs::sync::hd::Wallet> getSelectedXbtWallet(ReplyType replyType) const;
         bs::Address selectedAuthAddress(ReplyType replyType) const;
         std::vector<UTXO> selectedXbtInputs(ReplyType replyType) const;
         void submit(double price, const std::shared_ptr<SubmitQuoteReplyData>& replyData);
         void reserveBestUtxoSetAndSubmit(double quantity, double price,
            const std::shared_ptr<SubmitQuoteReplyData>& replyData, ReplyType replyType);
         void refreshSettlementDetails();


         std::set<std::string> activeQuoteSubmits_;
         std::map<std::string, std::map<std::string, std::array<bs::Address, static_cast<size_t>(AddressType::Max) + 1>>> addresses_;

         int walletFlags_{ 0 };
         std::vector<bs::sync::HDWalletData> wallets_;
         std::unordered_map<std::string, double>   balances_;
         std::unordered_map<std::string, std::shared_ptr<SubmitQuoteReplyData>>  pendingReservations_;
      };

   }  //namespace ui
}  //namespace bs

#endif // __RFQ_TICKET_XBT_H__
