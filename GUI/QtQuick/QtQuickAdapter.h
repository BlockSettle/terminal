/*

***********************************************************************************
* Copyright (C) 2020 - 2022, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef QT_QUICK_ADAPTER_H
#define QT_QUICK_ADAPTER_H

#include <set>
#include <QObject>
#include "Address.h"
#include "ApiAdapter.h"
#include "Wallets/SignContainer.h"
#include "ThreadSafeClasses.h"
#include "UiUtils.h"

namespace bs {
   namespace gui {
      namespace qt {
         class MainWindow;
      }
   }
}
namespace BlockSettle {
   namespace Common {
      class ArmoryMessage_AddressHistory;
      class ArmoryMessage_FeeLevelsResponse;
      class ArmoryMessage_ZCInvalidated;
      class ArmoryMessage_ZCReceived;
      class LedgerEntries;
      class OnChainTrackMessage_AuthAddresses;
      class OnChainTrackMessage_AuthState;
      class SignerMessage_SignTxResponse;
      class WalletsMessage_AuthKey;
      class WalletsMessage_ReservedUTXOs;
      class WalletsMessage_TXDetailsResponse;
      class WalletsMessage_UtxoListResponse;
      class WalletsMessage_WalletBalances;
      class WalletsMessage_WalletData;
      class WalletsMessage_WalletsListResponse;
   }
   namespace Terminal {
      class AssetsMessage_Balance;
      class AssetsMessage_SubmittedAuthAddresses;
      class BsServerMessage_LoginResult;
      class BsServerMessage_Orders;
      class BsServerMessage_StartLoginResult;
      class MatchingMessage_LoggedIn;
      class Quote;
      class QuoteCancelled;
      class IncomingRFQ;
      class MatchingMessage_Order;
      class MktDataMessage_Prices;
      class SettlementMessage_FailedSettlement;
      class SettlementMessage_MatchedQuote;
      class SettlementMessage_PendingSettlement;
      class SettlementMessage_SettlementIds;
      class SettingsMessage_ArmoryServers;
      class SettingsMessage_SettingsResponse;
      class SettingsMessage_SignerServers;
   }
}

class BSTerminalSplashScreen;

class QtQuickAdapter : public QObject, public ApiBusAdapter, public bs::MainLoopRuner
{
   Q_OBJECT
   friend class GuiThread;
public:
   QtQuickAdapter(const std::shared_ptr<spdlog::logger> &);
   ~QtQuickAdapter() override;

   bool process(const bs::message::Envelope &) override;
   bool processBroadcast(const bs::message::Envelope&) override;

   Users supportedReceivers() const override { return { user_ }; }
   std::string name() const override { return "QtQuick"; }

   void run(int &argc, char **argv) override;

private:
   bool processSettings(const bs::message::Envelope &);
   bool processSettingsGetResponse(const BlockSettle::Terminal::SettingsMessage_SettingsResponse&);
   bool processSettingsState(const BlockSettle::Terminal::SettingsMessage_SettingsResponse&);
   bool processArmoryServers(const BlockSettle::Terminal::SettingsMessage_ArmoryServers&);
   bool processAdminMessage(const bs::message::Envelope &);
   bool processBlockchain(const bs::message::Envelope &);
   bool processSigner(const bs::message::Envelope &);
   bool processWallets(const bs::message::Envelope &);

   void requestInitialSettings();
   void updateSplashProgress();
   void splashProgressCompleted();
   void updateStates();

   void createWallet(bool primary);

   void processWalletLoaded(const bs::sync::WalletInfo &);
   bool processWalletData(const uint64_t msgId
      , const BlockSettle::Common::WalletsMessage_WalletData&);
   bool processWalletBalances(const bs::message::Envelope &
      , const BlockSettle::Common::WalletsMessage_WalletBalances &);
   bool processTXDetails(uint64_t msgId, const BlockSettle::Common::WalletsMessage_TXDetailsResponse &);
   bool processLedgerEntries(const BlockSettle::Common::LedgerEntries &);
   bool processAddressHist(const BlockSettle::Common::ArmoryMessage_AddressHistory&);
   bool processFeeLevels(const BlockSettle::Common::ArmoryMessage_FeeLevelsResponse&);
   bool processWalletsList(const BlockSettle::Common::WalletsMessage_WalletsListResponse&);
   bool processUTXOs(const BlockSettle::Common::WalletsMessage_UtxoListResponse&);
   bool processSignTX(const BlockSettle::Common::SignerMessage_SignTxResponse&);
   bool processZC(const BlockSettle::Common::ArmoryMessage_ZCReceived&);
   bool processZCInvalidated(const BlockSettle::Common::ArmoryMessage_ZCInvalidated&);
   bool processReservedUTXOs(const BlockSettle::Common::WalletsMessage_ReservedUTXOs&);

private:
   std::shared_ptr<spdlog::logger>        logger_;
   BSTerminalSplashScreen* splashScreen_{ nullptr };
   QObject* rootObj_{ nullptr };
   std::shared_ptr<bs::message::UserTerminal>   userSettings_, userWallets_;
   std::shared_ptr<bs::message::UserTerminal>   userBlockchain_, userSigner_;
   bool loadingDone_{ false };

   std::recursive_mutex mutex_;
   std::set<int>  createdComponents_;
   std::set<int>  loadingComponents_;
   int         armoryState_{ -1 };
   uint32_t    blockNum_{ 0 };
   int         signerState_{ -1 };
   std::string signerDetails_;
   bool  walletsReady_{ false };

   std::map<uint64_t, std::string>  walletGetMap_;
   std::unordered_map<std::string, bs::sync::WalletInfo> hdWallets_;
   std::set<uint64_t>   newZCs_;

   std::unordered_map<std::string, bs::network::Asset::Type>   assetTypes_;
   std::set<bs::message::SeqId>  needChangeAddrReqs_;
};


#endif	// QT_QUICK_ADAPTER_H
