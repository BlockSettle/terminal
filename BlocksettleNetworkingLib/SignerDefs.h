/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __SIGNER_DEFS_H__
#define __SIGNER_DEFS_H__

#include "Address.h"
#include "BtcDefinitions.h"
#include "HDPath.h"
#include "WalletEncryption.h"

#include "headless.pb.h"

using namespace Blocksettle::Communication;

namespace bs {
   namespace core {
      class WalletsManager;
      namespace hd {
         class Leaf;
      }
   }
}

namespace bs {
namespace signer {

   using RequestId = unsigned int;

   struct Limits {
      uint64_t    autoSignSpendXBT = UINT64_MAX;
      uint64_t    manualSpendXBT = UINT64_MAX;
      int         autoSignTimeS = 0;
      int         manualPassKeepInMemS = 0;

      Limits() {}
      Limits(uint64_t asXbt, uint64_t manXbt, int asTime, int manPwTime)
         : autoSignSpendXBT(asXbt), manualSpendXBT(manXbt), autoSignTimeS(asTime)
         , manualPassKeepInMemS(manPwTime) {}
   };

   enum class RunMode {
      fullgui,
      litegui,
      headless,
      cli
   };

   // Keep in sync with Blocksettle.Communication.signer.BindStatus
   enum class BindStatus
   {
      Inactive = 0,
      Succeed = 1,
      Failed = 2,
   };

   enum class AutoSignCategory
   {
      NotDefined = 0,
      RegularTx = 1,
      SettlementDealer = 2,
      SettlementRequestor = 3,
      SettlementOTC = 4,
      CreateLeaf = 5,
   };

} // signer

namespace sync {

   enum class WalletFormat {
      Unknown = 0,
      HD,
      Plain,
      Settlement
   };

   struct WalletInfo
   {
      static std::vector<bs::sync::WalletInfo> fromPbMessage(const headless::SyncWalletInfoResponse &response);

      WalletFormat   format;
      std::string id;
      std::string name;
      std::string description;
      NetworkType netType;
      bool        watchOnly;

      std::vector<bs::wallet::EncryptionType>   encryptionTypes;
      std::vector<BinaryData> encryptionKeys;
      bs::wallet::KeyRank     encryptionRank{ 0,0 };
   };

   struct HDWalletData
   {
      struct Leaf {
         std::string          id;
         bs::hd::Path         path;
         bool extOnly;
         BinaryData  extraData;
      };
      struct Group {
         bs::hd::CoinType  type;
         std::vector<Leaf> leaves;
         bool extOnly;
         BinaryData salt;
      };
      std::vector<Group>   groups;
   };

   struct AddressData
   {
      std::string index;
      bs::Address address;
      std::string comment;
   };

   struct TxCommentData
   {
      BinaryData  txHash;
      std::string comment;
   };

   struct WalletData
   {
      static WalletData fromPbMessage(const headless::SyncWalletResponse &response);

      //flag value, signifies the higest index entries are unset if not changed from UINT32_MAX
      unsigned int highestExtIndex = UINT32_MAX; 
      unsigned int highestIntIndex = UINT32_MAX;

      std::vector<AddressData>   addresses;
      std::vector<AddressData>   addrPool;
      std::vector<TxCommentData> txComments;
   };

   struct WatchingOnlyWallet
   {
      struct Address {
         std::string index;
         AddressEntryType  aet;
      };
      struct Leaf {
         std::string          id;
         bs::hd::Path         path;
         BinaryData           publicKey;
         BinaryData           chainCode;
         std::vector<Address> addresses;
      };
      struct Group {
         bs::hd::CoinType  type;
         std::vector<Leaf> leaves;
      };

      NetworkType netType = NetworkType::Invalid;
      std::string id;
      std::string name;
      std::string description;
      std::vector<Group>   groups;
   };

   headless::SyncWalletInfoResponse exportHDWalletsInfoToPbMessage(const std::shared_ptr<bs::core::WalletsManager> &walletsMgr);
   headless::SyncWalletResponse     exportHDLeafToPbMessage(const std::shared_ptr<bs::core::hd::Leaf> &leaf);

   bs::wallet::EncryptionType mapFrom(headless::EncryptionType encType);
   NetworkType mapFrom(headless::NetworkType netType);
   bs::sync::WalletFormat mapFrom(headless::WalletFormat format);

   headless::EncryptionType mapFrom(bs::wallet::EncryptionType encType);
   headless::NetworkType mapFrom(NetworkType netType);

}  //namespace sync

} // bs

#endif
