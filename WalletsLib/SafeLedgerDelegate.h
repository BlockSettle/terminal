#ifndef __SAFE_LEDGER_DELEGATE_H__
#define __SAFE_LEDGER_DELEGATE_H__

#include "SwigClient.h"

#include <vector>

class QMutex;

class SafeLedgerDelegate
{
public:
   SafeLedgerDelegate(const SwigClient::LedgerDelegate& delegate, std::atomic_flag &);
   ~SafeLedgerDelegate() noexcept = default;

   SafeLedgerDelegate(const SafeLedgerDelegate&) = delete;
   SafeLedgerDelegate& operator = (const SafeLedgerDelegate&) = delete;

   SafeLedgerDelegate(SafeLedgerDelegate&&) = delete;
   SafeLedgerDelegate& operator = (SafeLedgerDelegate&&) = delete;

   std::vector<ClientClasses::LedgerEntry> getHistoryPage(uint32_t id);

private:
   SwigClient::LedgerDelegate delegate_;
   std::atomic_flag        &  bdvLock_;
};

#endif // __SAFE_LEDGER_DELEGATE_H__
