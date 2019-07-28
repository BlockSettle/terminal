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
            Group(const bs::hd::Path &path, const std::string &walletName
               , const std::string &name, const std::string &desc
               , WalletSignerContainer *container, WalletCallbackTarget *wct
               , const std::shared_ptr<spdlog::logger> &logger
               , bool extOnlyAddresses = false)
               : signContainer_(container)
               , logger_(logger), path_(path)
               , walletName_(walletName), name_(name), desc_(desc)
               , extOnlyAddresses_(extOnlyAddresses)
               , wct_(wct) {}
            virtual ~Group() = default;

            size_t getNumLeaves() const { return leaves_.size(); }
            std::shared_ptr<hd::Leaf> getLeaf(bs::hd::Path::Elem) const;
            std::shared_ptr<hd::Leaf> getLeaf(const std::string &key) const;
            std::vector<std::shared_ptr<hd::Leaf>> getLeaves() const;
            std::vector<std::shared_ptr<bs::sync::Wallet>> getAllLeaves() const;
            std::shared_ptr<hd::Leaf> createLeaf(bs::hd::Path::Elem, const std::string &walletId);
            std::shared_ptr<hd::Leaf> createLeaf(const std::string &key, const std::string &walletId);
            virtual std::shared_ptr<hd::Leaf> newLeaf(const std::string &walletId) const;
            virtual bool addLeaf(const std::shared_ptr<hd::Leaf> &, bool signal = false);
            bool deleteLeaf(const std::shared_ptr<bs::sync::Wallet> &);
            bool deleteLeaf(const bs::hd::Path::Elem &);
            bool deleteLeaf(const std::string &key);
            static std::string nameForType(bs::hd::CoinType ct);

            virtual bs::core::wallet::Type type() const { return bs::core::wallet::Type::Bitcoin; }
            const bs::hd::Path &path() const { return path_; }
            bs::hd::Path::Elem index() const { return static_cast<bs::hd::Path::Elem>(path_.get(-1)); }
            std::string name() const { return name_; }
            std::string description() const { return desc_; }

            virtual void setUserId(const BinaryData &) {}

         protected:
            using cb_scan_notify = std::function<void(Group *, bs::hd::Path::Elem wallet, bool isValid)>;
            using cb_scan_read_last = std::function<unsigned int(const std::string &walletId)>;
            using cb_scan_write_last = std::function<void(const std::string &walletId, unsigned int idx)>;

            virtual void initLeaf(std::shared_ptr<hd::Leaf> &, const bs::hd::Path &) const;

         protected:
            WalletSignerContainer  *  signContainer_{};
            std::shared_ptr<spdlog::logger>  logger_;
            bs::hd::Path   path_;
            std::string    walletName_, name_, desc_;
            bool        extOnlyAddresses_;
            std::unordered_map<bs::hd::Path::Elem, std::shared_ptr<hd::Leaf>> leaves_;
            unsigned int   scanPortion_ = 200;
            WalletCallbackTarget *wct_{};
         };


         class AuthGroup : public Group
         {
         public:
            AuthGroup(const bs::hd::Path &path, const std::string &name
               , const std::string &desc, WalletSignerContainer *, WalletCallbackTarget *wct
               , const std::shared_ptr<spdlog::logger>& logger
               , bool extOnlyAddresses = false);

            bs::core::wallet::Type type() const override { return bs::core::wallet::Type::Authentication; }

            void setUserId(const BinaryData &usedId) override;

         protected:
            bool addLeaf(const std::shared_ptr<hd::Leaf> &, bool signal = false) override;
            std::shared_ptr<hd::Leaf> newLeaf(const std::string &walletId) const override;
            void initLeaf(std::shared_ptr<hd::Leaf> &, const bs::hd::Path &) const override;

            BinaryData  userId_;
            std::unordered_map<bs::hd::Path::Elem, std::shared_ptr<hd::Leaf>>  tempLeaves_;
         };


         class CCGroup : public Group
         {
         public:
            CCGroup(const bs::hd::Path &path, const std::string &name
               , const std::string &desc, WalletSignerContainer *container
               , WalletCallbackTarget *wct
               , const std::shared_ptr<spdlog::logger> &logger
               , bool extOnlyAddresses = false)
               : Group(path, name, nameForType(bs::hd::CoinType::BlockSettle_CC),
                  desc, container, wct, logger, extOnlyAddresses) {}

            bs::core::wallet::Type type() const override { return bs::core::wallet::Type::ColorCoin; }

         protected:
            std::shared_ptr<hd::Leaf> newLeaf(const std::string &walletId) const override;
         };

         class SettlementGroup : public Group
         {
         public:
            SettlementGroup(const bs::hd::Path &path, const std::string &name
               , const std::string &desc, WalletSignerContainer *container
               , WalletCallbackTarget *wct
               , const std::shared_ptr<spdlog::logger> &logger)
               : Group(path, name, nameForType(bs::hd::CoinType::BlockSettle_Settlement),
                  desc, container, wct, logger) {}

            bs::core::wallet::Type type() const override { return bs::core::wallet::Type::Settlement; }
            std::shared_ptr<hd::SettlementLeaf> getLeaf(const bs::Address &) const;
            void addMap(const BinaryData &addr, bs::hd::Path::Elem idx) { addrMap_[addr] = idx; }

         protected:
            std::shared_ptr<hd::Leaf> newLeaf(const std::string &walletId) const override;

         private:
            std::map<BinaryData, bs::hd::Path::Elem>  addrMap_;
         };

      }  //namespace hd
   }  //namespace sync
}  //namespace bs

#endif //BS_SYNC_HD_GROUP_H
