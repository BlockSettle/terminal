#ifndef __RFQ_DEALER_REPLY_H__
#define __RFQ_DEALER_REPLY_H__

#include <QTimer>
#include <QWidget>

#include <atomic>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include "CommonTypes.h"
#include "EncryptionUtils.h"
#include "FrejaREST.h"
#include "MetaData.h"
#include "UserScript.h"

namespace Ui {
    class RFQDealerReply;
};
namespace spdlog {
   class logger;
}
namespace bs {
   class DealerUtxoResAdapter;
   class SettlementAddressEntry;
   class Wallet;
}
class ApplicationSettings;
class AssetManager;
class AuthAddressManager;
class QuoteProvider;
class SelectedTransactionInputs;
class SignContainer;
class TransactionData;
class WalletsManager;
class CustomDoubleSpinBox;

QT_BEGIN_NAMESPACE
class QDoubleSpinBox;
class QPushButton;
QT_END_NAMESPACE

namespace bs {
   namespace network {
      struct QuoteNotification;
   }

   namespace ui {
      class RFQDealerReply : public QWidget
      {
         Q_OBJECT

      public:
         RFQDealerReply(QWidget* parent = nullptr);
         virtual ~RFQDealerReply();

         void init(const std::shared_ptr<spdlog::logger> logger
            , const std::shared_ptr<AuthAddressManager> &
            , const std::shared_ptr<AssetManager>& assetManager
            , const std::shared_ptr<QuoteProvider>& quoteProvider
            , const std::shared_ptr<ApplicationSettings> &
            , const std::shared_ptr<SignContainer> &
            , std::shared_ptr<MarketDataProvider> mdProvider);
         void setWalletsManager(const std::shared_ptr<WalletsManager> &walletsManager);

         std::shared_ptr<TransactionData> getTransactionData(const std::string &reqId) const;
         bool autoSign() const;

         CustomDoubleSpinBox* bidSpinBox() const;
         CustomDoubleSpinBox* offerSpinBox() const;

         QPushButton* pullButton() const;
         QPushButton* quoteButton() const;

      signals:
         void aqScriptLoaded(const QString &filename);
         void autoSignActivated(const SecureBinaryData &password, const QString &hdWalletId, bool active);
         void submitQuoteNotif(const network::QuoteNotification &);
         void pullQuoteNotif(const QString &reqId, const QString &reqSessToken);

      public slots:
         void setQuoteReqNotification(const network::QuoteReqNotification &, double indicBid, double indicAsk);
         void quoteReqNotifStatusChanged(const network::QuoteReqNotification &);
         void onQuoteReqNotification(const network::QuoteReqNotification &);
         void onQuoteReqCancelled(const QString &reqId, bool byUser);
         void onQuoteReqRejected(const QString &reqId);
         void onMDUpdate(bs::network::Asset::Type, const QString &security, bs::network::MDFields);
         void onBestQuotePrice(const QString reqId, double price, bool own);
         void onAutoSignActivated();
         void onAutoSignStateChanged(const std::string &walletId, bool active, const std::string &error);
         void onCelerConnected();
         void onCelerDisconnected();

      private slots:
         void initUi();
         void priceChanged();
         void updateSubmitButton();
         void submitButtonClicked();
         void pullButtonClicked();
         void showCoinControl();
         void aqFillHistory();
         void aqScriptChanged(int curIndex);
         void onAqScriptLoaded(const QString &filename);
         void walletSelected(int index);
         void onTransactionDataChanged();
         void aqStateChanged(int state);
         void aqTick();
         void onAQReply(const QString &reqId, double price);
         void onAQPull(const QString &reqId);
         void onReservedUtxosChanged(const std::string &walletId, const std::vector<UTXO> &);
         void onQuoteReceived(const bs::network::Quote &);
         void onQuoteNotifCancelled(const QString &reqId);
         void onOrderUpdated(const bs::network::Order &);
         void onHDLeafCreated(unsigned int id, BinaryData pubKey, BinaryData chainCode, std::string walletId);
         void onCreateHDWalletError(unsigned int id, std::string error);
         void onSignerStateUpdated();
         void startSigning();
         void updateAutoSignState();

      protected:
         bool eventFilter(QObject *watched, QEvent *evt) override;

      private:
         Ui::RFQDealerReply* ui_;
         std::shared_ptr<spdlog::logger>        logger_;
         std::shared_ptr<WalletsManager>        walletsManager_;
         std::shared_ptr<AuthAddressManager>    authAddressManager_;
         std::shared_ptr<AssetManager>          assetManager_;
         std::shared_ptr<QuoteProvider>         quoteProvider_;
         std::shared_ptr<ApplicationSettings>   appSettings_;
         std::shared_ptr<SignContainer>         signingContainer_;
         std::shared_ptr<MarketDataProvider>    mdProvider_;

         std::shared_ptr<bs::Wallet>   curWallet_;
         std::shared_ptr<bs::Wallet>   prevWallet_;
         std::shared_ptr<bs::Wallet>   ccWallet_;
         std::shared_ptr<bs::Wallet>   xbtWallet_;

         std::unordered_map<std::string, double>   sentNotifs_;
         network::QuoteReqNotification    currentQRN_;
         std::shared_ptr<TransactionData> transactionData_;
         bool dealerSellXBT_;
         std::shared_ptr<SelectedTransactionInputs>   ccCoinSel_;

         double   indicBid_;
         double   indicAsk_;
         std::atomic_bool     autoUpdatePrices_;
         std::vector<wallet::EncryptionType> walletEncTypes_;
         std::vector<SecureBinaryData>       walletEncKeys_;
         bs::wallet::KeyRank  walletEncRank_;
         unsigned int         leafCreateReqId_ = 0;

         std::string product_;
         std::string baseProduct_;

         AutoQuoter *   aq_;
         bool           aqLoaded_ = false;
         bool           aqEnabled_ = false;
         bool           celerConnected_ = false;
         std::unordered_map<std::string, QObject *>   aqObjs_;
         std::unordered_map<std::string, bs::network::QuoteReqNotification>   aqQuoteReqs_;
         std::unordered_map<std::string, std::shared_ptr<TransactionData> >   aqTxData_;
         QTimer         aqTimer_;

         std::shared_ptr<DealerUtxoResAdapter>  utxoAdapter_;

         std::unordered_map<std::string, double>   bestQPrices_;
         struct MDInfo {
            double   bidPrice;
            double   askPrice;
            double   lastPrice;
         };
         std::unordered_map<std::string, MDInfo>  mdInfo_;

      private:
         void reset();
         void validateGUI();
         void updateRespQuantity();
         void updateQuoteReqNotification(const network::QuoteReqNotification &);
         double getPrice() const;
         double getValue() const;
         double getAmount() const;
         std::shared_ptr<bs::Wallet> getCurrentWallet() const { return curWallet_; }
         void setCurrentWallet(const std::shared_ptr<bs::Wallet> &);
         std::shared_ptr<bs::Wallet> getCCWallet(const std::string &cc);
         std::shared_ptr<bs::Wallet> getXbtWallet();
         bs::Address getRecvAddress() const;
         void initAQ(const QString &filename);
         void setBalanceOk(bool ok);
         void updateRecvAddresses();
         bool checkBalance() const;
         QDoubleSpinBox *getActivePriceWidget() const;
         void updateUiWalletFor(const bs::network::QuoteReqNotification &qrn);
         network::QuoteNotification submitReply(const std::shared_ptr<TransactionData> transData
            , const network::QuoteReqNotification &qrn, double price);
      };

   }  //namespace ui
}  //namespace bs

#endif // __RFQ_TICKET_XBT_H__
