/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef BS_SYNC_HD_GROUP_H__
#define BS_SYNC_HD_GROUP_H__

#include <unordered_map>
#include "CoreWallet.h"
#include "HDPath.h"
#include "SyncHDLeaf.h"

namespace spdlog {
   class logger;
}
class WalletSignerContainer;

namespace bs {
   namespace sync {
      class Wallet;

      namespace hd {
         class Wallet;

         class Group
         {
            friend class bs::sync::hd::Wallet;

         public:
            Group(bs::hd::Path::Elem index, const std::string &walletName
               , const std::string &name, const std::string &desc
               , WalletSignerContainer *container, WalletCallbackTarget *wct
               , const std::shared_ptr<spdlog::logger> &logger
               , bool extOnlyAddresses = false)
               : signContainer_(container)
               , logger_(logger), index_(index)
               , walletName_(walletName), name_(name), desc_(desc)
               , extOnlyAddresses_(extOnlyAddresses)
               , wct_(wct) {}
            virtual ~Group() = default;

            size_t getNumLeaves() const { return leaves_.size(); }
            std::shared_ptr<hd::Leaf> getLeaf(const bs::hd::Path &) const;
//            std::shared_ptr<hd::Leaf> getLeaf(const std::string &key) const;
            std::vector<std::shared_ptr<hd::Leaf>> getLeaves() const;
            std::vector<std::shared_ptr<bs::sync::Wallet>> getAllLeaves() const;
            std::shared_ptr<hd::Leaf> createLeaf(const bs::hd::Path &, const std::string &walletId);
            virtual std::shared_ptr<hd::Leaf> newLeaf(const std::string &walletId) const;
            virtual bool addLeaf(const std::shared_ptr<hd::Leaf> &, bool signal = false);
            bool deleteLeaf(const std::shared_ptr<bs::sync::Wallet> &);
            bool deleteLeaf(const bs::hd::Path &);
            static std::string nameForType(bs::hd::CoinType ct);

            virtual bs::core::wallet::Type type() const { return bs::core::wallet::Type::Bitcoin; }
            bs::hd::Path::Elem index() const { return index_; }
            std::string name() const { return name_; }
            std::string description() const { return desc_; }

            virtual void setUserId(const BinaryData &) {}

            void resetWCT();
         protected:
            using cb_scan_notify = std::function<void(Group *, bs::hd::Path::Elem wallet, bool isValid)>;
            using cb_scan_read_last = std::function<unsigned int(const std::string &walletId)>;
            using cb_scan_write_last = std::function<void(const std::string &walletId, unsigned int idx)>;

            virtual void initLeaf(std::shared_ptr<hd::Leaf> &, const bs::hd::Path &) const;

         protected:
            WalletSignerContainer  *  signContainer_{};
            std::shared_ptr<spdlog::logger>  logger_;
            bs::hd::Path::Elem   index_;
            std::string    walletName_, name_, desc_;
            bool        extOnlyAddresses_;
            std::map<bs::hd::Path, std::shared_ptr<hd::Leaf>>  leaves_;
            unsigned int   scanPortion_ = 200;
            WalletCallbackTarget *wct_{};
         };


         class AuthGroup : public Group
         {
         public:
            AuthGroup(const std::string &name, const std::string &desc
               , WalletSignerContainer *, WalletCallbackTarget *wct
               , const std::shared_ptr<spdlog::logger>& logger
               , bool extOnlyAddresses = false);

            bs::core::wallet::Type type() const override { return bs::core::wallet::Type::Authentication; }

            void setUserId(const BinaryData &usedId) override;

         protected:
            std::shared_ptr<hd::Leaf> newLeaf(const std::string &walletId) const override;
            void initLeaf(std::shared_ptr<hd::Leaf> &, const bs::hd::Path &) const override;

         private:
            BinaryData  userId_;
         };


         class CCGroup : public Group
         {
         public:
            CCGroup(const std::string &name, const std::string &desc
               , WalletSignerContainer *container
               , WalletCallbackTarget *wct
               , const std::shared_ptr<spdlog::logger> &logger
               , bool extOnlyAddresses = false)
               : Group(bs::hd::CoinType::BlockSettle_CC, name
                  , nameForType(bs::hd::CoinType::BlockSettle_CC)
                  , desc, container, wct, logger, extOnlyAddresses) {}

            bs::core::wallet::Type type() const override { return bs::core::wallet::Type::ColorCoin; }

         protected:
            std::shared_ptr<hd::Leaf> newLeaf(const std::string &walletId) const override;
         };

         class SettlementGroup : public Group
         {
         public:
            SettlementGroup(const std::string &name, const std::string &desc
               , WalletSignerContainer *container, WalletCallbackTarget *wct
               , const std::shared_ptr<spdlog::logger> &logger)
               : Group(bs::hd::CoinType::BlockSettle_Settlement, name
                  , nameForType(bs::hd::CoinType::BlockSettle_Settlement)
                  , desc, container, wct, logger) {}

            bs::core::wallet::Type type() const override { return bs::core::wallet::Type::Settlement; }
            std::shared_ptr<hd::SettlementLeaf> getLeaf(const bs::Address &) const;
            void addMap(const BinaryData &addr, const bs::hd::Path &path);

         protected:
            std::shared_ptr<hd::Leaf> newLeaf(const std::string &walletId) const override;

         private:
            std::map<BinaryData, bs::hd::Path>  addrMap_;
         };

      }  //namespace hd
   }  //namespace sync
}  //namespace bs

#endif //BS_SYNC_HD_GROUP_H
