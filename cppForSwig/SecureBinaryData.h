////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//  Copyright (C) 2011-2015, Armory Technologies, Inc.                        //
//  Distributed under the GNU Affero General Public License (AGPL v3)         //
//  See LICENSE-ATI or http://www.gnu.org/licenses/agpl.html                  //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

#ifndef SECUREBINARYDATA_H_
#define SECUREBINARYDATA_H_

#include "BinaryData.h"

// This is used to attempt to keep keying material out of swap
// I am stealing this from bitcoin 0.4.0 src, serialize.h
#if defined(_MSC_VER) || defined(__MINGW32__)
   // Note that VirtualLock does not provide this as a guarantee on Windows,
   // but, in practice, memory that has been VirtualLock'd almost never gets written to
   // the pagefile except in rare circumstances where memory is extremely low.
#include <windows.h>
#include "leveldb_windows_port\win32_posix\mman.h"
//#define mlock(p, n) VirtualLock((p), (n));
//#define munlock(p, n) VirtualUnlock((p), (n));
#else
#include <sys/mman.h>
#include <limits.h>
/* This comes from limits.h if it's not defined there set a sane default */
#ifndef PAGESIZE
#include <unistd.h>
#define PAGESIZE sysconf(_SC_PAGESIZE)
#endif
#define mlock(a,b) \
     mlock(((void *)(((size_t)(a)) & (~((PAGESIZE)-1)))),\
     (((((size_t)(a)) + (b) - 1) | ((PAGESIZE) - 1)) + 1) - (((size_t)(a)) & (~((PAGESIZE) - 1))))
#define munlock(a,b) \
     munlock(((void *)(((size_t)(a)) & (~((PAGESIZE)-1)))),\
     (((((size_t)(a)) + (b) - 1) | ((PAGESIZE) - 1)) + 1) - (((size_t)(a)) & (~((PAGESIZE) - 1))))
#endif

////////////////////////////////////////////////////////////////////////////////
// Make sure that all crypto information is handled with page-locked data,
// and overwritten when it's destructor is called.  For simplicity, we will
// use this data type for all crypto data, even for data values that aren't
// really sensitive.  We can use the SecureBinaryData(bdObj) to convert our 
// regular strings/BinaryData objects to secure objects
//
class SecureBinaryData : public BinaryData
{
public:
   // We want regular BinaryData, but page-locked and secure destruction
   SecureBinaryData(void) : BinaryData()
   {
      lockData();
   }
   SecureBinaryData(size_t sz) : BinaryData(sz)
   {
      lockData();
   }
   SecureBinaryData(BinaryData const & data) : BinaryData(data)
   {
      lockData();
   }
   SecureBinaryData(uint8_t const * inData, size_t sz) : BinaryData(inData, sz)
   {
      lockData();
   }
   SecureBinaryData(uint8_t const * d0, uint8_t const * d1) : BinaryData(d0, d1)
   {
      lockData();
   }
   SecureBinaryData(std::string const & str) : BinaryData(str)
   {
      lockData();
   }
   SecureBinaryData(BinaryDataRef const & bdRef) : BinaryData(bdRef)
   {
      lockData();
   }

   ~SecureBinaryData(void) { destroy(); }

   SecureBinaryData(SecureBinaryData&& mv) : BinaryData()
   {
      data_ = move(mv.data_);
   }

   // These methods are definitely inherited, but SWIG needs them here if they
   // are to be used from python
   uint8_t const *   getPtr(void)  const { return BinaryData::getPtr(); }
   uint8_t       *   getPtr(void) { return BinaryData::getPtr(); }
   size_t            getSize(void) const { return BinaryData::getSize(); }
   SecureBinaryData  copy(void)    const { return SecureBinaryData(getPtr(), getSize()); }

   std::string toHexStr(bool BE = false) const { return BinaryData::toHexStr(BE); }
   std::string toBinStr(void) const { return BinaryData::toBinStr(); }

   SecureBinaryData(SecureBinaryData const & sbd2) :
      BinaryData(sbd2.getPtr(), sbd2.getSize()) {
      lockData();
   }


   void resize(size_t sz) { BinaryData::resize(sz);  lockData(); }
   void reserve(size_t sz) { BinaryData::reserve(sz); lockData(); }


   BinaryData    getRawCopy(void) const { return BinaryData(getPtr(), getSize()); }
   BinaryDataRef getRawRef(void) { return BinaryDataRef(getPtr(), getSize()); }

   SecureBinaryData copySwapEndian(size_t pos1 = 0, size_t pos2 = 0) const;

   SecureBinaryData & append(SecureBinaryData & sbd2);
   SecureBinaryData & operator=(SecureBinaryData const & sbd2);
   SecureBinaryData   operator+(SecureBinaryData & sbd2) const;
   //uint8_t const & operator[](size_t i) const {return BinaryData::operator[](i);}
   bool operator==(SecureBinaryData const & sbd2) const;

   SecureBinaryData getHash256(void) const;
   SecureBinaryData getHash160(void) const;

   void lockData(void)
   {
      if (getSize() > 0)
         mlock(getPtr(), getSize());
   }

   void destroy(void)
   {
      if (getSize() > 0)
      {
         fill(0x00);
         munlock(getPtr(), getSize());
      }
      resize(0);
   }

   void XOR(const SecureBinaryData& rhs)
   {
      if (getSize() > rhs.getSize())
         throw std::runtime_error("invalid right hand statment length");

      for (unsigned i = 0; i < getSize(); i++)
      {
         auto val = getPtr() + i;
         *val ^= *(rhs.getPtr() + i);
      }
   }

};

#endif