////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//  Copyright (C) 2011-2015, Armory Technologies, Inc.                        //
//  Distributed under the GNU Affero General Public License (AGPL v3)         //
//  See LICENSE-ATI or http://www.gnu.org/licenses/agpl.html                  //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////
#include "HistoryPager.h"

using namespace std;

uint32_t HistoryPager::txnPerPage_ = 100;

////////////////////////////////////////////////////////////////////////////////
void HistoryPager::addPage(vector<shared_ptr<Page>>& pages,
   uint32_t count, uint32_t bottom, uint32_t top)
{
   pages.push_back(make_shared<Page>(count, bottom, top));
}

////////////////////////////////////////////////////////////////////////////////
shared_ptr<map<BinaryData, LedgerEntry>> HistoryPager::getPageLedgerMap(
   function< map<BinaryData, TxIOPair>(uint32_t, uint32_t) > getTxio,
   function< map<BinaryData, LedgerEntry>(
      const map<BinaryData, TxIOPair>&, uint32_t, uint32_t) > buildLedgers,
   uint32_t pageId, unsigned updateID,
   map<BinaryData, TxIOPair>* txioMap)
{
   if (!isInitialized_->load(memory_order_relaxed))
   {
      LOGERR << "Uninitialized history";
      throw std::runtime_error("Uninitialized history");
   }

   auto pagesLocal = atomic_load_explicit(&pages_, memory_order_acquire);
   if (pagesLocal == nullptr)
      return nullptr;

   if (pageId >= pagesLocal->size())
      return nullptr;

   auto& page = (*pagesLocal)[pageId];

   if (updateID != UINT32_MAX && page->updateID_ == updateID)
   {
      //already loaded this page
      return page->pageLedgers_.get();
   }

   page->pageLedgers_.clear();

   //load page's block range from ssh and build ledgers
   if (txioMap != nullptr)
   {
      *txioMap = getTxio(page->blockStart_, page->blockEnd_);
      page->pageLedgers_.update(
         buildLedgers(*txioMap, page->blockStart_, page->blockEnd_));
   }
   else
   {
      auto&& txio = getTxio(page->blockStart_, page->blockEnd_);
      page->pageLedgers_.update(
         buildLedgers(txio, page->blockStart_, page->blockEnd_));
   }

   if(updateID != UINT32_MAX)
      page->updateID_ = updateID;
   return page->pageLedgers_.get();
}

////////////////////////////////////////////////////////////////////////////////
shared_ptr<map<BinaryData, LedgerEntry>> HistoryPager::getPageLedgerMap(
   uint32_t pageId)
{
   if (!isInitialized_->load(memory_order_relaxed))
   {
      LOGERR << "Uninitialized history";
      throw std::runtime_error("Uninitialized history");
   }

   auto pagesLocal = atomic_load_explicit(&pages_, memory_order_acquire);
   if (pagesLocal == nullptr)
      return nullptr;

   if (pageId >= pagesLocal->size())
      return nullptr;

   auto& page = (*pagesLocal)[pageId];

   if (page->pageLedgers_.size() != 0)
   {
      //already loaded this page
      return page->pageLedgers_.get();
   }
   else
   {
      return nullptr;
   }
}


////////////////////////////////////////////////////////////////////////////////
bool HistoryPager::mapHistory(
   function< map<uint32_t, uint32_t>(void)> getSSHsummary)
{
   //grab the ssh summary for the pager. This is a map, referencing the amount
   //of txio per block for the given address.
   
   map<uint32_t, uint32_t> newSummary;
   
   try
   {
      newSummary = move(getSSHsummary());
   }
   catch (AlreadyPagedException&)
   {
      return false;
   }

   reset();
   SSHsummary_.clear();
   SSHsummary_ = move(newSummary);
   auto newPages = make_shared<vector<shared_ptr<Page>>>();
   
   if (SSHsummary_.size() == 0)
   {
      addPage(*newPages, 0, 0, UINT32_MAX);
      atomic_store_explicit(&pages_, newPages, memory_order_release);
      isInitialized_->store(true, memory_order_relaxed);
      return true;
   }

   auto histIter = SSHsummary_.crbegin();
   uint32_t threshold = 0;
   uint32_t top = UINT32_MAX;

   while (histIter != SSHsummary_.crend())
   {
      threshold += histIter->second;

      if (threshold > txnPerPage_)
      {
         addPage(*newPages, threshold, histIter->first, top);

         threshold = 0;
         top = histIter->first - 1;
      }

      ++histIter;
   }

   if (threshold != 0)
      addPage(*newPages, threshold, 0, top);

   //sort pages canonically then store
   sortPages(*newPages);
   atomic_store_explicit(&pages_, newPages, memory_order_release);

   //mark as initialized
   isInitialized_->store(true, memory_order_relaxed);
   return true;
}

////////////////////////////////////////////////////////////////////////////////
uint32_t HistoryPager::getPageBottom(uint32_t id) const
{
   if (!isInitialized_->load(memory_order_relaxed))
      return 0;

   auto pagesLocal = atomic_load_explicit(&pages_, memory_order_acquire);
   if (pagesLocal == nullptr)
      return 0;

   if (id < pagesLocal->size())
      return (*pagesLocal)[id]->blockStart_;

   return 0;
}

////////////////////////////////////////////////////////////////////////////////
size_t HistoryPager::getPageCount(void) const
{
   if (!isInitialized_->load(memory_order_relaxed))
      return 0;

   auto pagesLocal = atomic_load_explicit(&pages_, memory_order_acquire);
   if (pagesLocal == nullptr)
      return 0;

   return pagesLocal->size();
}

////////////////////////////////////////////////////////////////////////////////
uint32_t HistoryPager::getRangeForHeightAndCount(
   uint32_t height, uint32_t count) const
{
   if (!isInitialized_->load(memory_order_relaxed))
   {
      LOGERR << "Uninitialized history";
      throw std::runtime_error("Uninitialized history");
   }

   auto pagesLocal = atomic_load_explicit(&pages_, memory_order_acquire);
   if (pagesLocal == nullptr)
      return 0;
   
   uint32_t total = 0;
   uint32_t top = 0;

   for (const auto& page : *pagesLocal)
   {
      if (page->blockEnd_ > height)
      {
         total += page->count_;
         top = page->blockEnd_;

         if (total > count)
            break;
      }
   }

   return top;
}

////////////////////////////////////////////////////////////////////////////////
uint32_t HistoryPager::getBlockInVicinity(uint32_t blk) const
{
   if (!isInitialized_->load(memory_order_relaxed))
   {
      LOGERR << "Uninitialized history";
      throw std::runtime_error("Uninitialized history");
   }

   uint32_t blkDiff = UINT32_MAX;
   uint32_t blkHeight = UINT32_MAX;

   for (auto& txioRange : SSHsummary_)
   {
      //look for txio summary with closest block
      uint32_t diff = abs(int(txioRange.first - blk));
      if (diff == 0)
         return txioRange.first;
      else if (diff < blkDiff)
      {
         blkHeight = txioRange.first;
         blkDiff = diff;
      }
   }

   return blkHeight;
}

////////////////////////////////////////////////////////////////////////////////
uint32_t HistoryPager::getPageIdForBlockHeight(uint32_t blk) const
{
   if (!isInitialized_->load(memory_order_relaxed))
   {
      LOGERR << "Uninitialized history";
      throw std::runtime_error("Uninitialized history");
   }

   unsigned i = 0;

   auto pagesLocal = atomic_load_explicit(&pages_, memory_order_acquire);
   if (pagesLocal == nullptr)
      return 0;

   for (auto& page : *pagesLocal)
   {
      if (blk >= page->blockStart_ && blk <= page->blockEnd_)
         return i;

      ++i;
   }

   return 0;
}

////////////////////////////////////////////////////////////////////////////////
void HistoryPager::sortPages(vector<shared_ptr<Page>>& pages)
{
   std::sort(pages.begin(), pages.end(), Page::comparator);
}