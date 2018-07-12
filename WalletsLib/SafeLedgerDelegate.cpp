#include "SafeLedgerDelegate.h"

#include "PyBlockDataManager.h"

#include "DbHeader.h"
#include "EncryptionUtils.h"
#include "FastLock.h"
#include <QDebug>
#include <exception>

SafeLedgerDelegate::SafeLedgerDelegate(const SwigClient::LedgerDelegate& delegate, std::atomic_flag &bdvLock)
   : delegate_(delegate)
   , bdvLock_(bdvLock)
{}

std::vector<ClientClasses::LedgerEntry> SafeLedgerDelegate::getHistoryPage(uint32_t id)
{
   std::vector<ClientClasses::LedgerEntry> result;

   try {
      FastLock lock(bdvLock_);
      return delegate_.getHistoryPage(id);
   } catch (const SocketError& e) {
      qDebug() << QLatin1String("[SafeLedgerDelegate::getHistoryPage] SocketError: ") << QString::fromStdString(e.what());
   } catch (const DbErrorMsg& e) {
      qDebug() << QLatin1String("[SafeLedgerDelegate::getHistoryPage] DbErrorMsg: ") << QString::fromStdString(e.what());
   } catch (const std::exception& e) {
      qDebug() << QLatin1String("[SafeLedgerDelegate::getHistoryPage] exception: ") << QString::fromStdString(e.what());
   } catch (...) {
      qDebug() << QLatin1String("[SafeLedgerDelegate::getHistoryPage] exception.");
   }

   return std::vector<ClientClasses::LedgerEntry>{};
}
