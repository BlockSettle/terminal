#ifndef BS_SYNC_PLAIN_WALLET_H
#define BS_SYNC_PLAIN_WALLET_H

#include <memory>
#include <unordered_map>
#include <QObject>
#include <BinaryData.h>

#include "SyncWallet.h"


namespace spdlog {
   class logger;
};

class WalletSignerContainer;

namespace bs {
   namespace sync {

      // A base wallet that can be used by other wallets, or for very basic
      // functionality (e.g., creating a bare wallet that can be registered and get
      // info on addresses added to the wallet). The wallet may or may not be able
      // to access the wallet DB.
      class PlainWallet : public Wallet
      {
      public:
         PlainWallet(const std::string &walletId, const std::string &name, const std::string &desc
            , WalletSignerContainer *, const std::shared_ptr<spdlog::logger> &logger);
         ~PlainWallet() override;

         PlainWallet(const PlainWallet&) = delete;
         PlainWallet(PlainWallet&&) = delete;
         PlainWallet& operator = (const PlainWallet&) = delete;
         PlainWallet& operator = (PlainWallet&&) = delete;

         int addAddress(const bs::Address &, const std::string &index, AddressEntryType
            , bool sync = true) override;
         bool containsAddress(const bs::Address &addr) override;

         const std::string& walletId() const override { return walletId_; }
         std::string description() const override { return desc_; }
         void setDescription(const std::string &desc) override { desc_ = desc; }
         bs::core::wallet::Type type() const override { return bs::core::wallet::Type::Bitcoin; }

         void getNewExtAddress(const CbAddress &, AddressEntryType aet) override;
         void getNewIntAddress(const CbAddress &cb, AddressEntryType aet) override {
            getNewExtAddress(cb, aet);
         }
         size_t getUsedAddressCount() const override { return usedAddresses_.size(); }
         std::string getAddressIndex(const bs::Address &) override;
         bool addressIndexExists(const std::string &index) const override;

         bool deleteRemotely() override;

         virtual void merge(const std::shared_ptr<Wallet>)
         {
            throw std::runtime_error("not implemented yet. not sure is necessary");
         }

      protected:
         std::vector<BinaryData> getAddrHashes() const override;

      protected:
         mutable std::set<BinaryData>  addrPrefixedHashes_;

      private:
         int addressIndex(const bs::Address &) const;

      private:
         std::string    walletId_;
         std::string    desc_;
      };

   }  //namespace sync
}  //namespace bs

#endif // BS_SYNC_PLAIN_WALLET_H
