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
      class SignerMessage_CreateWalletRequest;
      class SignerMessage_DialogRequest;
      class SignerMessage_ExtendAddrChain;
      class SignerMessage_GetSettlPayinAddr;
      class SignerMessage_SetSettlementId;
      class SignerMessage_SignSettlementTx;
      class SignerMessage_SignTxRequest;
      class SignerMessage_SyncAddresses;
      class SignerMessage_SyncAddressComment;
      class SignerMessage_SyncNewAddresses;
      class SignerMessage_SyncTxComment;
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

   bool process(const bs::message::Envelope &) override;
   bool processBroadcast(const bs::message::Envelope&) override;

   Users supportedReceivers() const override { return { user_ }; }
   std::string name() const override { return "Signer"; }

   std::unique_ptr<SignerClient> createClient() const;

private:
   void start();

   // HCT overrides
   void walletsChanged() override;
   void onReady() override;
   void walletsReady() override;
   void newWalletPrompt() override;
   void autoSignStateChanged(bs::error::ErrorCode
      , const std::string& walletId) override;

   bool processOwnRequest(const bs::message::Envelope &
      , const BlockSettle::Common::SignerMessage &);
   bool processSignerSettings(const BlockSettle::Terminal::SettingsMessage_SignerServer &);
   bool processNewKeyResponse(bool);
   bool sendComponentLoading();

   bool processStartWalletSync(const bs::message::Envelope &);
   bool processSyncAddresses(const bs::message::Envelope &
      , const BlockSettle::Common::SignerMessage_SyncAddresses &);
   bool processSyncNewAddresses(const bs::message::Envelope &
      , const BlockSettle::Common::SignerMessage_SyncNewAddresses &);
   bool processExtendAddrChain(const bs::message::Envelope &
      , const BlockSettle::Common::SignerMessage_ExtendAddrChain &);
   bool processSyncWallet(const bs::message::Envelope &, const std::string &walletId);
   bool processSyncHdWallet(const bs::message::Envelope &, const std::string &walletId);
   bool processSyncAddrComment(const BlockSettle::Common::SignerMessage_SyncAddressComment &);
   bool processSyncTxComment(const BlockSettle::Common::SignerMessage_SyncTxComment &);
   bool processGetRootPubKey(const bs::message::Envelope &, const std::string &walletId);
   bool processDelHdRoot(const std::string &walletId);
   bool processDelHdLeaf(const std::string &walletId);
   bool processSignTx(const bs::message::Envelope&
      , const BlockSettle::Common::SignerMessage_SignTxRequest&);
   bool processResolvePubSpenders(const bs::message::Envelope&
      , const bs::core::wallet::TXSignRequest&);
   bool processAutoSignRequest(const bs::message::Envelope&
      , const BlockSettle::Common::SignerMessage_AutoSign&);
   bool processDialogRequest(const bs::message::Envelope&
      , const BlockSettle::Common::SignerMessage_DialogRequest&);
   bool processCreateWallet(const bs::message::Envelope&
      , const BlockSettle::Common::SignerMessage_CreateWalletRequest&);

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
