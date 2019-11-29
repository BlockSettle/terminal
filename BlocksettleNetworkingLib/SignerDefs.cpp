/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "SignerDefs.h"
#include "CoreWalletsManager.h"
#include "CoreHDWallet.h"

NetworkType bs::sync::mapFrom(headless::NetworkType netType)
{
   switch (netType) {
   case headless::MainNetType:   return NetworkType::MainNet;
   case headless::TestNetType:   return NetworkType::TestNet;
   default:    return NetworkType::Invalid;
   }
}

bs::sync::WalletFormat bs::sync::mapFrom(headless::WalletFormat format)
{
   switch (format) {
   case headless::WalletFormatHD:         return bs::sync::WalletFormat::HD;
   case headless::WalletFormatPlain:      return bs::sync::WalletFormat::Plain;
   case headless::WalletFormatSettlement: return bs::sync::WalletFormat::Settlement;
   case headless::WalletFormatUnknown:
   default:    return bs::sync::WalletFormat::Unknown;
   }
}

bs::sync::WalletData bs::sync::WalletData::fromPbMessage(const headless::SyncWalletResponse &response)
{
   bs::sync::WalletData result;

   result.highestExtIndex = response.highest_ext_index();
   result.highestIntIndex = response.highest_int_index();

   for (int i = 0; i < response.addresses_size(); ++i) {
      const auto addrInfo = response.addresses(i);
      const auto addr = bs::Address::fromAddressString(addrInfo.address());
      if (addr.isNull()) {
         continue;
      }
      result.addresses.push_back({ addrInfo.index(), std::move(addr)
         , addrInfo.comment() });
   }
   for (int i = 0; i < response.addrpool_size(); ++i) {
      const auto addrInfo = response.addrpool(i);
      const auto addr = bs::Address::fromAddressString(addrInfo.address());
      if (addr.isNull()) {
         continue;
      }
      result.addrPool.push_back({ addrInfo.index(), std::move(addr), "" });
   }
   for (int i = 0; i < response.txcomments_size(); ++i) {
      const auto txInfo = response.txcomments(i);
      result.txComments.push_back({ txInfo.txhash(), txInfo.comment() });
   }

   return result;
}

std::vector<bs::sync::WalletInfo> bs::sync::WalletInfo::fromPbMessage(const headless::SyncWalletInfoResponse &response)
{
   std::vector<bs::sync::WalletInfo> result;
   for (int i = 0; i < response.wallets_size(); ++i) {
      const auto walletInfoPb = response.wallets(i);
      bs::sync::WalletInfo walletInfo;

      walletInfo.format = mapFrom(walletInfoPb.format());
      walletInfo.id = walletInfoPb.id();
      walletInfo.name = walletInfoPb.name();
      walletInfo.description = walletInfoPb.description();
      walletInfo.netType = mapFrom(walletInfoPb.nettype());
      walletInfo.watchOnly = walletInfoPb.watching_only();

      for (int i = 0; i < walletInfoPb.encryptiontypes_size(); ++i) {
         const auto encType = walletInfoPb.encryptiontypes(i);
         walletInfo.encryptionTypes.push_back(bs::sync::mapFrom(encType));
      }
      for (int i = 0; i < walletInfoPb.encryptionkeys_size(); ++i) {
         const auto encKey = walletInfoPb.encryptionkeys(i);
         walletInfo.encryptionKeys.push_back(encKey);
      }
      walletInfo.encryptionRank = { walletInfoPb.keyrank().m(), walletInfoPb.keyrank().n() };

      result.push_back(walletInfo);
   }
   return result;
}

bs::wallet::EncryptionType bs::sync::mapFrom(headless::EncryptionType encType)
{
   switch (encType) {
   case headless::EncryptionTypePassword: return bs::wallet::EncryptionType::Password;
   case headless::EncryptionTypeAutheID:  return bs::wallet::EncryptionType::Auth;
   case headless::EncryptionTypeUnencrypted:
   default:    return bs::wallet::EncryptionType::Unencrypted;
   }
}

headless::SyncWalletInfoResponse bs::sync::exportHDWalletsInfoToPbMessage(const std::shared_ptr<bs::core::WalletsManager> &walletsMgr)
{
   headless::SyncWalletInfoResponse response;
   assert(walletsMgr);

   if (!walletsMgr) {
      return response;
   }

   for (size_t i = 0; i < walletsMgr->getHDWalletsCount(); ++i) {
      auto wallet = response.add_wallets();
      const auto hdWallet = walletsMgr->getHDWallet(i);
      wallet->set_format(headless::WalletFormatHD);
      wallet->set_id(hdWallet->walletId());
      wallet->set_name(hdWallet->name());
      wallet->set_description(hdWallet->description());
      wallet->set_nettype(mapFrom(hdWallet->networkType()));
      wallet->set_watching_only(hdWallet->isWatchingOnly());

      for (const auto &encType : hdWallet->encryptionTypes()) {
         wallet->add_encryptiontypes(bs::sync::mapFrom(encType));
      }
      for (const auto &encKey : hdWallet->encryptionKeys()) {
         wallet->add_encryptionkeys(encKey.toBinStr());
      }
      auto keyrank = wallet->mutable_keyrank();
      keyrank->set_m(hdWallet->encryptionRank().m);
      keyrank->set_n(hdWallet->encryptionRank().n);
   }
   return response;
}

headless::EncryptionType bs::sync::mapFrom(bs::wallet::EncryptionType encType)
{
   switch (encType) {
   case bs::wallet::EncryptionType::Password:   return headless::EncryptionTypePassword;
   case bs::wallet::EncryptionType::Auth:       return headless::EncryptionTypeAutheID;
   case bs::wallet::EncryptionType::Unencrypted:
   default:       return headless::EncryptionTypeUnencrypted;
   }
}

headless::NetworkType bs::sync::mapFrom(NetworkType netType)
{
   switch (netType) {
   case NetworkType::MainNet: return headless::MainNetType;
   case NetworkType::TestNet:
   default:    return headless::TestNetType;
   }
}

headless::SyncWalletResponse bs::sync::exportHDLeafToPbMessage(const std::shared_ptr<bs::core::hd::Leaf> &leaf)
{
   headless::SyncWalletResponse response;
   response.set_walletid(leaf->walletId());

   response.set_highest_ext_index(leaf->getExtAddressCount());
   response.set_highest_int_index(leaf->getIntAddressCount());

   for (const auto &addr : leaf->getUsedAddressList()) {
      const auto comment = leaf->getAddressComment(addr);
      const auto index = leaf->getAddressIndex(addr);
      auto addrData = response.add_addresses();
      addrData->set_address(addr.display());
      addrData->set_index(index);
      if (!comment.empty()) {
         addrData->set_comment(comment);
      }
   }
   const auto &pooledAddresses = leaf->getPooledAddressList();
   for (const auto &addr : pooledAddresses) {
      const auto index = leaf->getAddressIndex(addr);
      auto addrData = response.add_addrpool();
      addrData->set_address(addr.display());
      addrData->set_index(index);
   }
   for (const auto &txComment : leaf->getAllTxComments()) {
      auto txCommData = response.add_txcomments();
      txCommData->set_txhash(txComment.first.toBinStr());
      txCommData->set_comment(txComment.second);
   }
   return response;
}
