////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//  Copyright (C) 2011-2015, Armory Technologies, Inc.                        //
//  Distributed under the GNU Affero General Public License (AGPL v3)         //
//  See LICENSE-ATI or http://www.gnu.org/licenses/agpl.html                  //
//                                                                            //
//                                                                            //
//  Copyright (C) 2016, goatpig                                               //            
//  Distributed under the MIT license                                         //
//  See LICENSE-MIT or https://opensource.org/licenses/MIT                    //                                   
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

#include "DBUtils.h"
#ifdef _WIN32
#include <Windows.h>
#include <io.h>
#include <fcntl.h>
#include <dirent_win32.h>
#include <ShlObj.h>

#define unlink _unlink
#define access _access
#else
#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <wordexp.h>
#endif

using namespace std;

////////////////////////////////////////////////////////////////////////////////
const BinaryData DBUtils::ZeroConfHeader_ = BinaryData::CreateFromHex("FFFF");

////////////////////////////////////////////////////////////////////////////////
BLKDATA_TYPE DBUtils::readBlkDataKey(BinaryRefReader & brr,
   uint32_t & height,
   uint8_t  & dupID)
{
   uint16_t tempTxIdx;
   uint16_t tempTxOutIdx;
   return readBlkDataKey(brr, height, dupID, tempTxIdx, tempTxOutIdx);
}

////////////////////////////////////////////////////////////////////////////////
BLKDATA_TYPE DBUtils::readBlkDataKey(BinaryRefReader & brr,
   uint32_t & height,
   uint8_t  & dupID,
   uint16_t & txIdx)
{
   uint16_t tempTxOutIdx;
   return readBlkDataKey(brr, height, dupID, txIdx, tempTxOutIdx);
}

////////////////////////////////////////////////////////////////////////////////
BLKDATA_TYPE DBUtils::readBlkDataKey(BinaryRefReader & brr,
   uint32_t & height,
   uint8_t  & dupID,
   uint16_t & txIdx,
   uint16_t & txOutIdx)
{
   uint8_t prefix = brr.get_uint8_t();
   if (prefix != (uint8_t)DB_PREFIX_TXDATA)
   {
      height = 0xffffffff;
      dupID = 0xff;
      txIdx = 0xffff;
      txOutIdx = 0xffff;
      return NOT_BLKDATA;
   }

   return readBlkDataKeyNoPrefix(brr, height, dupID, txIdx, txOutIdx);
}

////////////////////////////////////////////////////////////////////////////////
BLKDATA_TYPE DBUtils::readBlkDataKeyNoPrefix(
   BinaryRefReader & brr,
   uint32_t & height,
   uint8_t  & dupID)
{
   uint16_t tempTxIdx;
   uint16_t tempTxOutIdx;
   return readBlkDataKeyNoPrefix(brr, height, dupID, tempTxIdx, tempTxOutIdx);
}

////////////////////////////////////////////////////////////////////////////////
BLKDATA_TYPE DBUtils::readBlkDataKeyNoPrefix(
   BinaryRefReader & brr,
   uint32_t & height,
   uint8_t  & dupID,
   uint16_t & txIdx)
{
   uint16_t tempTxOutIdx;
   return readBlkDataKeyNoPrefix(brr, height, dupID, txIdx, tempTxOutIdx);
}

////////////////////////////////////////////////////////////////////////////////
BLKDATA_TYPE DBUtils::readBlkDataKeyNoPrefix(
   BinaryRefReader & brr,
   uint32_t & height,
   uint8_t  & dupID,
   uint16_t & txIdx,
   uint16_t & txOutIdx)
{
   BinaryData hgtx = brr.get_BinaryData(4);
   height = hgtxToHeight(hgtx);
   dupID = hgtxToDupID(hgtx);

   if (brr.getSizeRemaining() == 0)
   {
      txIdx = 0xffff;
      txOutIdx = 0xffff;
      return BLKDATA_HEADER;
   }
   else if (brr.getSizeRemaining() == 2)
   {
      txIdx = brr.get_uint16_t(BE);
      txOutIdx = 0xffff;
      return BLKDATA_TX;
   }
   else if (brr.getSizeRemaining() == 4)
   {
      txIdx = brr.get_uint16_t(BE);
      txOutIdx = brr.get_uint16_t(BE);
      return BLKDATA_TXOUT;
   }
   else
   {
      LOGERR << "Unexpected bytes remaining: " << brr.getSizeRemaining();
      return NOT_BLKDATA;
   }
}

////////////////////////////////////////////////////////////////////////////////
string DBUtils::getPrefixName(uint8_t prefixInt)
{
   return getPrefixName((DB_PREFIX)prefixInt);
}

////////////////////////////////////////////////////////////////////////////////
string DBUtils::getPrefixName(DB_PREFIX pref)
{
   switch (pref)
   {
   case DB_PREFIX_DBINFO:    return string("DBINFO");
   case DB_PREFIX_TXDATA:    return string("TXDATA");
   case DB_PREFIX_SCRIPT:    return string("SCRIPT");
   case DB_PREFIX_TXHINTS:   return string("TXHINTS");
   case DB_PREFIX_TRIENODES: return string("TRIENODES");
   case DB_PREFIX_HEADHASH:  return string("HEADHASH");
   case DB_PREFIX_HEADHGT:   return string("HEADHGT");
   case DB_PREFIX_UNDODATA:  return string("UNDODATA");
   default:                  return string("<unknown>");
   }
}

/////////////////////////////////////////////////////////////////////////////
bool DBUtils::checkPrefixByteWError(BinaryRefReader & brr,
   DB_PREFIX prefix,
   bool rewindWhenDone)
{
   uint8_t oneByte = brr.get_uint8_t();
   bool out;
   if (oneByte == (uint8_t)prefix)
      out = true;
   else
   {
      LOGERR << "Unexpected prefix byte: "
         << "Expected: " << getPrefixName(prefix)
         << "Received: " << getPrefixName(oneByte);
      out = false;
   }

   if (rewindWhenDone)
      brr.rewind(1);

   return out;
}

/////////////////////////////////////////////////////////////////////////////
bool DBUtils::checkPrefixByte(BinaryRefReader & brr,
   DB_PREFIX prefix,
   bool rewindWhenDone)
{
   uint8_t oneByte = brr.get_uint8_t();
   bool out = (oneByte == (uint8_t)prefix);

   if (rewindWhenDone)
      brr.rewind(1);

   return out;
}

/////////////////////////////////////////////////////////////////////////////
BinaryData DBUtils::getBlkDataKey(uint32_t height,
   uint8_t  dup)
{
   BinaryWriter bw(5);
   bw.put_uint8_t(DB_PREFIX_TXDATA);
   bw.put_BinaryData(heightAndDupToHgtx(height, dup));
   return bw.getData();
}

/////////////////////////////////////////////////////////////////////////////
BinaryData DBUtils::getBlkDataKey(uint32_t height,
   uint8_t  dup,
   uint16_t txIdx)
{
   BinaryWriter bw(7);
   bw.put_uint8_t(DB_PREFIX_TXDATA);
   bw.put_BinaryData(heightAndDupToHgtx(height, dup));
   bw.put_uint16_t(txIdx, BE);
   return bw.getData();
}

/////////////////////////////////////////////////////////////////////////////
BinaryData DBUtils::getBlkDataKey(uint32_t height,
   uint8_t  dup,
   uint16_t txIdx,
   uint16_t txOutIdx)
{
   BinaryWriter bw(9);
   bw.put_uint8_t(DB_PREFIX_TXDATA);
   bw.put_BinaryData(heightAndDupToHgtx(height, dup));
   bw.put_uint16_t(txIdx, BE);
   bw.put_uint16_t(txOutIdx, BE);
   return bw.getData();
}

/////////////////////////////////////////////////////////////////////////////
BinaryData DBUtils::getBlkDataKeyNoPrefix(uint32_t height,
   uint8_t  dup)
{
   return heightAndDupToHgtx(height, dup);
}

/////////////////////////////////////////////////////////////////////////////
BinaryData DBUtils::getBlkDataKeyNoPrefix(uint32_t height,
   uint8_t  dup,
   uint16_t txIdx)
{
   BinaryWriter bw(6);
   bw.put_BinaryData(heightAndDupToHgtx(height, dup));
   bw.put_uint16_t(txIdx, BE);
   return bw.getData();
}

/////////////////////////////////////////////////////////////////////////////
BinaryData DBUtils::getBlkDataKeyNoPrefix(uint32_t height,
   uint8_t  dup,
   uint16_t txIdx,
   uint16_t txOutIdx)
{
   BinaryWriter bw(8);
   bw.put_BinaryData(heightAndDupToHgtx(height, dup));
   bw.put_uint16_t(txIdx, BE);
   bw.put_uint16_t(txOutIdx, BE);
   return bw.getData();
}

/////////////////////////////////////////////////////////////////////////////
uint32_t DBUtils::hgtxToHeight(const BinaryData& hgtx)
{
   return (READ_UINT32_BE(hgtx) >> 8);

}

/////////////////////////////////////////////////////////////////////////////
uint8_t DBUtils::hgtxToDupID(const BinaryData& hgtx)
{
   return (READ_UINT32_BE(hgtx) & 0x7f);
}

/////////////////////////////////////////////////////////////////////////////
BinaryData DBUtils::heightAndDupToHgtx(uint32_t hgt, uint8_t dup)
{
   uint32_t hgtxInt = (hgt << 8) | (uint32_t)dup;
   return WRITE_UINT32_BE(hgtxInt);
}

/////////////////////////////////////////////////////////////////////////////
BinaryData DBUtils::getFilterPoolKey(uint32_t filenum)
{
   uint32_t bucketKey = (DB_PREFIX_POOL << 24) | (uint32_t)filenum;
   return WRITE_UINT32_BE(bucketKey);
}

/////////////////////////////////////////////////////////////////////////////
BinaryData DBUtils::getMissingHashesKey(uint32_t id)
{
   BinaryData bd;
   bd.resize(4);

   id &= 0x00FFFFFF; //24bit ids top
   id |= DB_PREFIX_MISSING_HASHES << 24;
   
   auto keyPtr = (uint32_t*)bd.getPtr();
   *keyPtr = id;

   return bd;
}

/////////////////////////////////////////////////////////////////////////////
bool DBUtils::fileExists(const string& path, int mode)
{
#ifdef _WIN32
   return _access(path.c_str(), mode) == 0;
#else
      auto nixmode = F_OK;
      if (mode & 2)
         nixmode |= R_OK;
      if (mode & 4)
         nixmode |= W_OK;
      return access(path.c_str(), nixmode) == 0;
#endif
}

/////////////////////////////////////////////////////////////////////////////
void FileMap::unmap()
{
   if (filePtr_ != nullptr)
   {
#ifdef WIN32
      if (!UnmapViewOfFile(filePtr_))
         throw std::runtime_error("failed to unmap file");
#else
      if (munmap(filePtr_, size_))
         throw std::runtime_error("failed to unmap file");
#endif

      filePtr_ = nullptr;
   }
}

/////////////////////////////////////////////////////////////////////////////
FileMap DBUtils::getMmapOfFile(const string& path)
{
   int fd = 0;
   if (!DBUtils::fileExists(path, 2))
      throw runtime_error("file does not exist");

   FileMap fMap;

   try
   {
#ifdef _WIN32
      fd = _open(path.c_str(), _O_RDONLY | _O_BINARY);
      if (fd == -1)
         throw runtime_error("failed to open file");

      auto size = _lseek(fd, 0, SEEK_END);

      if (size == 0)
      {
         stringstream ss;
         ss << "empty block file under path: " << path;
         throw ss.str();
      }

      _lseek(fd, 0, SEEK_SET);
#else
      fd = open(path.c_str(), O_RDONLY);
      if (fd == -1)
         throw runtime_error("failed to open file");

      auto size = lseek(fd, 0, SEEK_END);

      if (size == 0)
      {
         stringstream ss;
         ss << "empty block file under path: " << path;
         throw ss.str();
      }

      lseek(fd, 0, SEEK_SET);
#endif
      fMap.size_ = size;

#ifdef _WIN32
      //create mmap
      auto fileHandle = (HANDLE)_get_osfhandle(fd);
      HANDLE mh;

      uint32_t sizelo = size & 0xffffffff;
      uint32_t sizehi = size >> 16 >> 16;


      mh = CreateFileMapping(fileHandle, NULL, PAGE_READONLY,
         sizehi, sizelo, NULL);
      if (!mh)
      {
         auto errorCode = GetLastError();
         stringstream errStr;
         errStr << "Failed to create map of file. Error Code: " <<
            errorCode << " (" << strerror(errorCode) << ")";
         throw runtime_error(errStr.str());
      }

      fMap.filePtr_ = (uint8_t*)MapViewOfFileEx(mh, FILE_MAP_READ, 0, 0, size, NULL);
      if (fMap.filePtr_ == nullptr)
      {
         auto errorCode = GetLastError();
         stringstream errStr;
         errStr << "Failed to create map of file. Error Code: " <<
            errorCode << " (" << strerror(errorCode) << ")";
         throw runtime_error(errStr.str());
      }

      CloseHandle(mh);
      _close(fd);
#else
      fMap.filePtr_ = (uint8_t*)mmap(0, size, PROT_READ, MAP_SHARED,
         fd, 0);
      if (fMap.filePtr_ == MAP_FAILED) {
         fMap.filePtr_ = NULL;
         stringstream errStr;
         errStr << "Failed to create map of file. Error Code: " <<
            errno << " (" << strerror(errno) << ")";
         cout << errStr.str() << endl;
         throw runtime_error(errStr.str());
      }

      close(fd);
#endif
      fd = 0;
   }
   catch (runtime_error &e)
   {
      if (fd != 0)
      {
#ifdef _WIN32
         _close(fd);
#else
         close(fd);
#endif
         fd = 0;
      }

      throw e;
   }

   return fMap;
}

////////////////////////////////////////////////////////////////////////////////
BinaryDataRef DBUtils::getDataRefForPacket(
   const BinaryDataRef& packet)
{
   BinaryRefReader brr(packet);
   auto len = brr.get_var_int();
   if (len != brr.getSizeRemaining())
      throw runtime_error("on disk data length mismatch");

   return brr.get_BinaryDataRef(brr.getSizeRemaining());
}

////////////////////////////////////////////////////////////////////////////////
struct stat DBUtils::getPathStat(const char* path, unsigned len)
{
   if (path == nullptr || len == 0)
      throw runtime_error("invalid path");

   if(strlen(path) != len)
      throw runtime_error("invalid path");

   if (access(path, 0) != 0)
      throw runtime_error("invalid path");

   struct stat status;
   stat(path, &status);
   return status;
}

////////////////////////////////////////////////////////////////////////////////
struct stat DBUtils::getPathStat(const string& path)
{
   return getPathStat(path.c_str(), path.size());
}

////////////////////////////////////////////////////////////////////////////////
bool DBUtils::isFile(const string& path)
{
   struct stat status;
   try
   {
      status = move(getPathStat(path));
   }
   catch (exception&)
   {
      return false;
   }

   return status.st_mode & S_IFREG;
}

////////////////////////////////////////////////////////////////////////////////
bool DBUtils::isDir(const string& path)
{
   struct stat status;
   try
   {
      status = move(getPathStat(path));
   }
   catch (exception&)
   {
      return false;
   }

   return status.st_mode & S_IFDIR;
}

////////////////////////////////////////////////////////////////////////////////
int DBUtils::removeDirectory(const string& path)
{
   if (!isDir(path))
      return -1;

   DIR* current_dir = opendir(path.c_str());
   if (current_dir == nullptr)
      return -1;

   //gather paths in dir
   vector<string> file_vec;
   dirent* filename = nullptr;
   while ((filename = readdir(current_dir)) != nullptr)
      file_vec.push_back(string(filename->d_name));

   string dot(".");
   string dotdot("..");
   vector<string> path_vec;
   for (auto val : file_vec)
   {
      if (val == dot || val == dotdot)
         continue;

      stringstream path_ss;
      path_ss << path << "/" << val;

      path_vec.push_back(path_ss.str());
   }

   closedir(current_dir);

   for (auto& filepath : path_vec)
   {
      if (isDir(filepath))
      {
         auto result = removeDirectory(filepath);
         if (result != 0)
            return result;

         continue;
      }

      auto result = unlink(filepath.c_str());
      if (result != 0)
         return result;
   }

#ifdef _WIN32
   if (RemoveDirectory(path.c_str()) == 0)
      return -1;
#else
   return rmdir(path.c_str());
#endif

   return 0;
}

////////////////////////////////////////////////////////////////////////////////
size_t DBUtils::getFileSize(const string& path)
{
   auto stat_struct = getPathStat(path);
   return stat_struct.st_size;
}

////////////////////////////////////////////////////////////////////////////////
void DBUtils::appendPath(string& base, const string& add)
{
   if (add.size() == 0)
      return;

   auto firstChar = add.c_str()[0];
   if (base.size() > 0)
   {
      auto lastChar = base.c_str()[base.size() - 1];
      if (firstChar != '\\' && firstChar != '/')
         if (lastChar != '\\' && lastChar != '/')
            base.append("/");
   }

   base.append(add);
}

////////////////////////////////////////////////////////////////////////////////
void DBUtils::expandPath(string& path)
{
   if (path.c_str()[0] != '~')
      return;

   //resolve ~
#ifdef _WIN32
   char* pathPtr = new char[MAX_PATH + 1];
   if (SHGetFolderPath(0, CSIDL_APPDATA, 0, 0, pathPtr) != S_OK)
   {
      delete[] pathPtr;
      throw runtime_error("failed to resolve appdata path");
   }

   string userPath(pathPtr);
   delete[] pathPtr;
#else
   wordexp_t wexp;
   wordexp("~", &wexp, 0);

   if (wexp.we_wordc == 0)
      throw runtime_error("failed to resolve home path");

   string userPath(wexp.we_wordv[0]);
   wordfree(&wexp);
#endif

   appendPath(userPath, path.substr(1));
   path = move(userPath);
}
