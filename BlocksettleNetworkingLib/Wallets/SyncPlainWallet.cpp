#include "SyncPlainWallet.h"
#include <spdlog/spdlog.h>
#include "WalletSignerContainer.h"

using namespace bs::sync;

PlainWallet::PlainWallet(const std::string &walletId, const std::string &name, const std::string &desc,
   WalletSignerContainer *container
   , const std::shared_ptr<spdlog::logger> &logger)
   : Wallet(container, logger), walletId_(walletId), desc_(desc)
{
   walletName_ = name;
}

PlainWallet::~PlainWallet() = default;

int PlainWallet::addAddress(const bs::Address &addr, const std::string &index
   , AddressEntryType aet, bool sync)
{
   const int id = Wallet::addAddress(addr, index, aet, sync);
   addrPrefixedHashes_.insert(addr.id());
   return id;
}

std::vector<BinaryData> PlainWallet::getAddrHashes() const
{
   if (addrPrefixedHashes_.empty()) {
      for (const auto &addr : usedAddresses_) {
         addrPrefixedHashes_.insert(addr.prefixed());
      }
   }
   std::vector<BinaryData> result;
   result.insert(result.end(), addrPrefixedHashes_.cbegin(), addrPrefixedHashes_.cend());
   return result;
}

int PlainWallet::addressIndex(const bs::Address &addr) const
{
   for (size_t i = 0; i < usedAddresses_.size(); ++i) {
      if (usedAddresses_[i] == addr) {
         return i;
      }
   }
   return -1;
}

std::string PlainWallet::getAddressIndex(const bs::Address &addr)
{
   const auto index = addressIndex(addr);
   if (index < 0) {
      return {};
   }
   return std::to_string(index);
}

bool PlainWallet::addressIndexExists(const std::string &index) const
{
   const auto id = std::stoi(index);
   return ((id >= 0) && (id < usedAddresses_.size()));
}

bool PlainWallet::containsAddress(const bs::Address &addr)
{
   return (addressIndex(addr) >= 0);
}

void PlainWallet::getNewExtAddress(const CbAddress &, AddressEntryType)
{  // should have a pool to return immediately
}

bool PlainWallet::deleteRemotely()
{
   return false;  //stub
}
