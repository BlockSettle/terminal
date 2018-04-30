#ifndef __SYSTEM_FILE_UTILS_H__
#define __SYSTEM_FILE_UTILS_H__

#include <string>

namespace SystemFileUtils
{
   using file_path_type = std::string;

   bool IsValidFilePath(const file_path_type& path);
   bool FileExist(const file_path_type& path);

   bool SetFileFlag(const file_path_type& path);
   bool ClearFileFlag(const file_path_type& path);

   bool FileCopy(const file_path_type& from, const file_path_type& to);
   bool RemoveFile(const file_path_type& path);
};

#endif // __SYSTEM_FILE_UTILS_H__