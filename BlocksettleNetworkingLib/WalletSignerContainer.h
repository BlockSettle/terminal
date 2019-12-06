/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __WALLET_SIGNER_CONTAINER_H__
#define __WALLET_SIGNER_CONTAINER_H__

#include "SignContainer.h"

// WalletSignerContainer -  meant to be the interface used by Wallets Manager
// all leafs/wallets managin stuff
// All other signer users will stick to SignContainer interface for signing operations
class WalletSignerContainer : public SignContainer
{
Q_OBJECT

public:
   WalletSignerContainer(const std::shared_ptr<spdlog::logger> &, OpMode opMode);
   ~WalletSignerContainer() noexcept = default;

public:
   virtual void syncWalletInfo(const std::function<void(std::vector<bs::sync::WalletInfo>)> &) = 0;
   virtual void syncHDWallet(const std::string &id, const std::function<void(bs::sync::HDWalletData)> &) = 0;
   virtual void syncWallet(const std::string &id, const std::function<void(bs::sync::WalletData)> &) = 0;
   virtual void syncAddressComment(const std::string &walletId, const bs::Address &, const std::string &) = 0;
   virtual void syncTxComment(const std::string &walletId, const BinaryData &, const std::string &) = 0;

   virtual void syncAddressBatch(const std::string &walletId,
      const std::set<BinaryData>& addrSet, std::function<void(bs::sync::SyncState)>) = 0;
   virtual void extendAddressChain(const std::string &walletId, unsigned count, bool extInt,
      const std::function<void(const std::vector<std::pair<bs::Address, std::string>> &)> &) = 0;

   virtual bs::signer::RequestId DeleteHDRoot(const std::string &rootWalletId) = 0;
   virtual bs::signer::RequestId DeleteHDLeaf(const std::string &leafWalletId) = 0;

   //settlement related methods
   virtual void createSettlementWallet(const bs::Address &authAddr
      , const std::function<void(const SecureBinaryData &)> &) = 0;
   virtual void setSettlementID(const std::string &walletId, const SecureBinaryData &id
      , const std::function<void(bool)> &) = 0;
   virtual void getSettlementPayinAddress(const std::string &walletID
      , const bs::core::wallet::SettlementData &
      , const std::function<void(bool, bs::Address)> &) = 0;
   virtual void getRootPubkey(const std::string &walletID
      , const std::function<void(bool, const SecureBinaryData &)> &) = 0;

   virtual void getChatNode(const std::string &walletID
      , const std::function<void(const BIP32_Node &)> &) = 0;

   using CreateHDLeafCb = std::function<void(bs::error::ErrorCode, const std::string &leafWalletId)>;
   virtual bool createHDLeaf(const std::string &rootWalletId, const bs::hd::Path &
      , const std::vector<bs::wallet::PasswordData> &pwdData = {}
      , bs::sync::PasswordDialogData dialogData = {}, const CreateHDLeafCb &cb = nullptr) = 0;

   using PromoteHDWalletCb = std::function<void(bs::error::ErrorCode, const std::string &leafWalletId)>;
   virtual bool promoteHDWallet(const std::string& rootWalletId, const BinaryData &userId
      , bs::sync::PasswordDialogData dialogData = {}, const PromoteHDWalletCb& cb = nullptr) = 0;

signals:
   void AuthLeafAdded(const std::string &walletId);
   // Notified from remote/local signer when wallets list is updated
   void walletsListUpdated();
   void walletsStorageDecrypted();
};

#endif // __WALLET_SIGNER_CONTAINER_H__
