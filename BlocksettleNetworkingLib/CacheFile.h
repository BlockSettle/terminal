/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __CACHE_FILE_H__
#define __CACHE_FILE_H__

#include <unordered_map>
#include <atomic>
#include <QObject>
#include <QReadWriteLock>
#include <QWaitCondition>
#include <QMutex>
#include <QThreadPool>
#include <QTimer>
#include <lmdbpp.h>
#include "BinaryData.h"
#include "TxClasses.h"


class CacheFile : public QObject
{
   Q_OBJECT

public:
   CacheFile(const std::string &filename, size_t nbElemLimit = 10000);
   ~CacheFile();

   void put(const BinaryData &key, const BinaryData &val);
   BinaryData get(const BinaryData &key) const;
   void stop();

protected:
   void read();
   void write();
   void saver();
   void purge();

private:
   const bool  inMem_;
   size_t      nbMaxElems_;
   LMDB     *  db_ = nullptr;
   std::shared_ptr<LMDBEnv>  dbEnv_;
   std::map<BinaryData, BinaryData> map_, mapModified_;
   mutable QReadWriteLock  rwLock_;
   mutable QWaitCondition  wcModified_;
   mutable QMutex          mtxModified_;
   std::atomic_bool        stopped_;
   QThreadPool             threadPool_;
   QTimer                  saveTimer_;
};


class TxCacheFile : protected CacheFile
{
public:
   TxCacheFile(const std::string &filename, size_t nbElemLimit = 10000)
      : CacheFile(filename, nbElemLimit) {}

   void put(const BinaryData &key, const Tx &tx) {
      if (!tx.isInitialized()) {
         return;
      }
      CacheFile::put(key, tx.serialize());
   }
   Tx get(const BinaryData &key) {
      const auto &data = CacheFile::get(key);
      if (data.isNull()) {
         return Tx{};
      }
      return Tx(data);
   }

   void stop() { CacheFile::stop(); }
};

#endif // __CACHE_FILE_H__
