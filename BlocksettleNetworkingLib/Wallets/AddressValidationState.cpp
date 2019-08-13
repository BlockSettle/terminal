#include "AddressValidationState.h"

////////////////////////////////////////////////////////////////////////////////

class PageState
{
public:
   PageState()
      : txCount_{0}
      , completedTxCount_{0}
   {}

   ~PageState() noexcept = default;

   PageState(const PageState&) = delete;
   PageState& operator = (const PageState&) = delete;
   PageState(PageState&&) = delete;
   PageState& operator = (PageState&&) = delete;

public:
   void SetTXCount(const uint64_t txCount)
   {
      assert(txCount_ == 0);

      txCount_ = txCount;
      completedTxCount_ = 0;
   }

   bool IsCompleted() const {
      return (txCount_ == completedTxCount_);
   }

   void OnTxCompleted()
   {
      assert(txCount_ != completedTxCount_);
      ++completedTxCount_;
   }

private:
   uint64_t                txCount_;
   std::atomic<uint64_t>   completedTxCount_;
};

////////////////////////////////////////////////////////////////////////////////

class AddressState
{
public:
   AddressState() = default;
   ~AddressState() noexcept = default;

   AddressState(const AddressState&) = delete;
   AddressState& operator = (const AddressState&) = delete;

   AddressState(AddressState&&) = delete;
   AddressState& operator = (AddressState&&) = delete;

public:
   bool IsCompleted() const
   {
      return !pagesMap_.empty() && pagesMap_.size() == completedPages_;
   }

   void SetPagesCout(const uint64_t pagesCount)
   {
      assert(pagesCount != 0);
      assert(pagesMap_.empty());

      for (uint64_t i=0; i < pagesCount; ++i) {
         pagesMap_.emplace(i, std::make_shared<PageState>());
      }
      completedPages_ = 0;
   }

   void SetTxCountOnPage(const uint64_t pageId, const uint64_t txCount)
   {
      assert(pageId < pagesMap_.size());

      auto it = pagesMap_.find(pageId);
      it->second->SetTXCount(txCount);

      // it is valid case that there might be no TX on history page.
      // this mean that page should be marked as completed
      if (it->second->IsCompleted()) {
         assert(completedPages_ < pagesMap_.size());

         ++completedPages_;
      }
   }

   void CompleteTxOnPage(const uint64_t pageId)
   {
      assert(pageId < pagesMap_.size());

      auto it = pagesMap_.find(pageId);
      it->second->OnTxCompleted();

      if (it->second->IsCompleted()) {
         assert(completedPages_ < pagesMap_.size());

         ++completedPages_;
      }
   }

private:
   std::atomic<uint64_t>                     completedPages_{0};
   // atomic inside PageState. atomic have disabled copy ctor, so we shoudl disable copy ctor ourselves, and so we need to wrap in smart pointer to use map
   std::unordered_map<uint64_t, std::shared_ptr<PageState>>   pagesMap_;
};

////////////////////////////////////////////////////////////////////////////////

AddressValidationState::AddressValidationState(const onValidationComletedCB& cb)
   : cb_{cb}
{
   assert(cb_);
}

void AddressValidationState::SetAddressList(const std::vector<bs::Address>& addressList)
{
   assert(!addressList.empty());

   addressStateMap_.clear();


   for (auto& address : addressList) {
      addressStateMap_.emplace(address, std::make_shared< AddressState>());
   }
   completedAddressesCount_ = 0;
}

void AddressValidationState::SetAddressPagesCount(const bs::Address& address, const uint64_t pagesCount)
{
   assert(!addressStateMap_.empty());

   auto it = addressStateMap_.find(address);
   assert(it != addressStateMap_.end());

   it->second->SetPagesCout(pagesCount);
}

void AddressValidationState::SetAddressPageTxCount(const bs::Address& address, const uint64_t pageId, const uint64_t txCount)
{
   assert(!addressStateMap_.empty());

   auto it = addressStateMap_.find(address);
   assert(it != addressStateMap_.end());

   it->second->SetTxCountOnPage(pageId, txCount);
   // if there are no history for address - it is valid case and this
   if (it->second->IsCompleted()) {
      ++completedAddressesCount_;

      if (completedAddressesCount_ == addressStateMap_.size()) {
         cb_();
      }
   }
}

void AddressValidationState::OnTxProcessed(const bs::Address& address, const uint64_t pageId)
{
   assert(!addressStateMap_.empty());

   auto it = addressStateMap_.find(address);
   assert(it != addressStateMap_.end());

   it->second->CompleteTxOnPage(pageId);
   if (it->second->IsCompleted()) {
      ++completedAddressesCount_;

      if (completedAddressesCount_ == addressStateMap_.size()) {
         cb_();
      }
   }
}

bool AddressValidationState::IsValidationStarted() const
{
   return !addressStateMap_.empty();
}

bool AddressValidationState::IsValidationCompleted() const
{
   return completedAddressesCount_ == addressStateMap_.size();
}
