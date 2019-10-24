#include <chrono>
#include <QtConcurrent/QtConcurrentRun>
#include "CacheFile.h"

namespace {

   // We need at least 150 MiB on the drive in order for LMDB to work. Add some
   // buffer because, while LMDB does bump the DB map size if half the size is hit
   // within a cycle, it's possible (albeit unlikely) that expansion can occur so
   // quickly that the storage is exhausted.
   //
   // From LMDB docs: The size should be a multiple of the OS page size
   const size_t kCacheFileMapSize = 150*1024*1024;

} // namespace

CacheFile::CacheFile(const std::string &filename, size_t nbElemLimit)
   : QObject(nullptr)
   , inMem_(filename.empty())
   , nbMaxElems_(nbElemLimit)
   , stopped_(false)
   , threadPool_(this)
   , saveTimer_(this)
{
   if (!inMem_) {
      dbEnv_ = std::make_shared<LMDBEnv>();
      dbEnv_->open(filename);
      dbEnv_->setMapSize(kCacheFileMapSize);
      db_ = new LMDB(dbEnv_.get(), "cache");

      threadPool_.setMaxThreadCount(1);
      read();
      QtConcurrent::run(&threadPool_, this, &CacheFile::saver);

      saveTimer_.setInterval(123 * 1000);
      connect(&saveTimer_, &QTimer::timeout, [this] {
         wcModified_.wakeOne();
      });
      saveTimer_.start();
   }
}

CacheFile::~CacheFile()
{
   stop();

   if (!inMem_ && db_) {
      db_->close();
      dbEnv_->close();
      delete db_;
      db_ = nullptr;
   }
}

void CacheFile::stop()
{
   stopped_ = true;
   if (inMem_) {
      return;
   }
   {
      QMutexLocker lock(&mtxModified_);
      wcModified_.wakeAll();
   }
   threadPool_.clear();
   threadPool_.waitForDone();
}

#define DB_PREFIX    0xDC

void CacheFile::read()
{
   LMDBEnv::Transaction tx(dbEnv_.get(), LMDB::ReadOnly);
   auto dbIter = db_->begin();

   BinaryWriter bwKey;
   bwKey.put_uint8_t(DB_PREFIX);
   CharacterArrayRef keyRef(bwKey.getSize(), bwKey.getData().getPtr());

   dbIter.seek(keyRef, LMDB::Iterator::Seek_GE);
   QWriteLocker lock(&rwLock_);

   while (dbIter.isValid()) {
      auto iterkey = dbIter.key();
      auto itervalue = dbIter.value();

      BinaryDataRef keyBDR((uint8_t*)iterkey.mv_data, iterkey.mv_size);
      BinaryDataRef valueBDR((uint8_t*)itervalue.mv_data, itervalue.mv_size);

      if (keyBDR.getSize() < 2) {
         break;
      }
      BinaryRefReader brrKey(keyBDR);
      auto prefix = brrKey.get_uint8_t();
      if (prefix != DB_PREFIX) {
         break;
      }
      const BinaryData key(brrKey.getCurrPtr(), brrKey.getSizeRemaining());

      BinaryRefReader brrVal(valueBDR);
      const BinaryData value(brrVal.getCurrPtr(), brrVal.getSizeRemaining());
      if (!key.isNull()) {
         map_.emplace(key, value);
      }

      dbIter.advance();
   }
}

void CacheFile::write()
{
   QWriteLocker lockMap(&rwLock_);
   LMDBEnv::Transaction tx(dbEnv_.get(), LMDB::ReadWrite);
   QMutexLocker lockMapModif(&mtxModified_);
   for (const auto &entry : mapModified_) {
      BinaryWriter bwKey;
      bwKey.put_uint8_t(DB_PREFIX);
      bwKey.put_BinaryData(entry.first);

      CharacterArrayRef keyRef(bwKey.getData().getSize(), bwKey.getData().getPtr());
      CharacterArrayRef dataRef(entry.second.getSize(), entry.second.getPtr());
      db_->insert(keyRef, dataRef);
      map_[entry.first] = entry.second;
   }
   mapModified_.clear();
}

void CacheFile::saver()
{
   const std::chrono::duration<double> minSaveDuration(23.0);
   size_t nbElemsThreshold = 100;
   auto start = std::chrono::system_clock::now();

   while (!stopped_) {
      purge();

      {
         QMutexLocker lock(&mtxModified_);
         wcModified_.wait(&mtxModified_);

         if (stopped_ || mapModified_.empty()) {
            continue;
         }

         auto curTime = std::chrono::system_clock::now();
         std::chrono::duration<double> diff = curTime - start;
         if ((diff < minSaveDuration) && (mapModified_.size() < nbElemsThreshold)) {
            continue;
         }

         start = curTime;
      }

      write();
   }
   write();    // final flush
}

void CacheFile::purge()
{  // simple purge - without LRU/MRU counters
   {
      QReadLocker lock(&rwLock_);
      if (map_.size() < nbMaxElems_) {
         return;
      }
   }
   QWriteLocker lock(&rwLock_);
   LMDBEnv::Transaction tx(dbEnv_.get(), LMDB::ReadWrite);
   while (!stopped_ && (map_.size() >= nbMaxElems_)) {
      const auto entry = map_.begin();
      BinaryWriter bwKey;
      bwKey.put_uint8_t(DB_PREFIX);
      bwKey.put_BinaryData(entry->first);

      CharacterArrayRef keyRef(bwKey.getData().getSize(), bwKey.getData().getPtr());
      db_->erase(keyRef);
      map_.erase(entry->first);
   }
}

BinaryData CacheFile::get(const BinaryData &key) const
{
   QReadLocker lockMap(&rwLock_);
   auto it = map_.find(key);
   if (it == map_.end()) {
      if (inMem_) {
         return {};
      }
      else {
         QMutexLocker lockMapModif(&mtxModified_);
         it = mapModified_.find(key);
         if (it == mapModified_.end()) {
            return {};
         }
         return it->second;
      }
   }
   return it->second;
}

void CacheFile::put(const BinaryData &key, const BinaryData &val)
{
   if (inMem_) {
      QWriteLocker lock(&rwLock_);
      map_[key] = val;
   }
   else {
      QMutexLocker lock(&mtxModified_);
      mapModified_[key] = val;
      wcModified_.wakeOne();
   }
}
