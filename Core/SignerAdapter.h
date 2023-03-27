/*

***********************************************************************************
* Copyright (C) 2020 - 2021, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef SIGNER_ADAPTER_H
#define SIGNER_ADAPTER_H

#include "BtcDefinitions.h"
#include "FutureValue.h"
#include "Wallets/HeadlessContainer.h"
#include "Message/Adapter.h"
#include "ThreadSafeContainers.h"

namespace spdlog {
   class logger;
}
namespace bs {
   namespace core {
      class WalletsManager;
   }
}
namespace BlockSettle {
   namespace Common {
      class SignerMessage;
      class SignerMessage_AutoSign;
      class SignerMessage_ChangeWalletPassword;
      class SignerMessage_CreateWalletRequest;
      class SignerMessage_DialogRequest;
      class SignerMessage_ExportWoWalletRequest;
      class SignerMessage_ExtendAddrChain;
      class SignerMessage_GetSettlPayinAddr;
      class SignerMessage_ImportHWWallet;
      class SignerMessage_SetSettlementId;
      class SignerMessage_SignSettlementTx;
      class SignerMessage_SignTxRequest;
      class SignerMessage_SyncAddresses;
      class SignerMessage_SyncAddressComment;
      class SignerMessage_SyncNewAddresses;
      class SignerMessage_SyncTxComment;
      class SignerMessage_WalletRequest;
   }
   namespace Terminal {
      class SettingsMessage_SignerServer;
   }
}
class SignerClient;
class WalletSignerContainer;

class SignerAdapter : public bs::message::Adapter, public SignerCallbackTarget
{
public:
   SignerAdapter(const std::shared_ptr<spdlog::logger> &
      , const std::shared_ptr<WalletSignerContainer> &signer = nullptr);
   ~SignerAdapter() override = default;

   bs::message::ProcessingResult process(const bs::message::Envelope &) override;
   bool processBroadcast(const bs::message::Envelope&) override;

   Users supportedReceivers() const override { return { user_ }; }
   std::string name() const override { return "Signer"; }

   std::unique_ptr<SignerClient> createClient() const;

private:
   void start();

   // HCT overrides
   void walletsChanged(bool rescan = false) override;
   void onReady() override;
   void walletsReady() override;
   void newWalletPrompt() override;
   void autoSignStateChanged(bs::error::ErrorCode
      , const std::string& walletId) override;

   bs::message::ProcessingResult processOwnRequest(const bs::message::Envelope &
      , const BlockSettle::Common::SignerMessage &);
   bs::message::ProcessingResult processSignerSettings(const BlockSettle::Terminal::SettingsMessage_SignerServer &);
   bs::message::ProcessingResult processNewKeyResponse(bool);
   bool sendComponentLoading();

   bs::message::ProcessingResult processStartWalletSync(const bs::message::Envelope &);
   bs::message::ProcessingResult processSyncAddresses(const bs::message::Envelope &
      , const BlockSettle::Common::SignerMessage_SyncAddresses &);
   bs::message::ProcessingResult processSyncNewAddresses(const bs::message::Envelope &
      , const BlockSettle::Common::SignerMessage_SyncNewAddresses &);
   bs::message::ProcessingResult processExtendAddrChain(const bs::message::Envelope &
      , const BlockSettle::Common::SignerMessage_ExtendAddrChain &);
   bs::message::ProcessingResult processSyncWallet(const bs::message::Envelope &, const std::string &walletId);
   bs::message::ProcessingResult processSyncHdWallet(const bs::message::Envelope &, const std::string &walletId);
   bs::message::ProcessingResult processSyncAddrComment(const BlockSettle::Common::SignerMessage_SyncAddressComment &);
   bs::message::ProcessingResult processSyncTxComment(const BlockSettle::Common::SignerMessage_SyncTxComment &);
   bs::message::ProcessingResult processGetRootPubKey(const bs::message::Envelope &, const std::string &walletId);
   bs::message::ProcessingResult processDelHdRoot(const std::string &walletId);
   bs::message::ProcessingResult processDelHdLeaf(const std::string &walletId);
   bs::message::ProcessingResult processSignTx(const bs::message::Envelope&
      , const BlockSettle::Common::SignerMessage_SignTxRequest&);
   bs::message::ProcessingResult processResolvePubSpenders(const bs::message::Envelope&
      , const bs::core::wallet::TXSignRequest&);
   bs::message::ProcessingResult processAutoSignRequest(const bs::message::Envelope&
      , const BlockSettle::Common::SignerMessage_AutoSign&);
   bs::message::ProcessingResult processDialogRequest(const bs::message::Envelope&
      , const BlockSettle::Common::SignerMessage_DialogRequest&);
   bs::message::ProcessingResult processCreateWallet(const bs::message::Envelope&, bool rescan
      , const BlockSettle::Common::SignerMessage_CreateWalletRequest&);
   bs::message::ProcessingResult processImportHwWallet(const bs::message::Envelope&
      , const BlockSettle::Common::SignerMessage_ImportHWWallet&);
   bs::message::ProcessingResult processDeleteWallet(const bs::message::Envelope&
      , const BlockSettle::Common::SignerMessage_WalletRequest&);
   bs::message::ProcessingResult processExportWoWallet(const bs::message::Envelope&
      , const BlockSettle::Common::SignerMessage_ExportWoWalletRequest&);
   bs::message::ProcessingResult processChangeWalletPass(const bs::message::Envelope&
      , const BlockSettle::Common::SignerMessage_ChangeWalletPassword&);
   bs::message::ProcessingResult processGetWalletSeed(const bs::message::Envelope&
      , const BlockSettle::Common::SignerMessage_WalletRequest&);
   bs::message::ProcessingResult processImportWoWallet(const bs::message::Envelope&
      , const std::string& filename);
   bs::message::ProcessingResult processWalletRename(const bs::message::Envelope&
      , const std::string& walletId, const std::string& newName);

private:
   std::shared_ptr<spdlog::logger>        logger_;
   std::shared_ptr<bs::message::User>     user_;
   NetworkType netType_{NetworkType::Invalid};
   std::string walletsDir_;
   std::shared_ptr<bs::core::WalletsManager> walletsMgr_;
   std::shared_ptr<WalletSignerContainer>    signer_;

   std::shared_ptr<FutureValue<bool>>  connFuture_;
   std::string    curServerId_;
   std::string    connKey_;

   bs::ThreadSafeMap<uint64_t, std::shared_ptr<bs::message::User>>   requests_;
   std::unordered_map<std::string, bs::message::Envelope>   autoSignRequests_;
   SecureBinaryData passphrase_;
};

#endif	// SIGNER_ADAPTER_H
