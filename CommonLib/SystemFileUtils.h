/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __SYSTEM_FILE_UTILS_H__
#define __SYSTEM_FILE_UTILS_H__

#include <string>
#include <vector>

namespace SystemFileUtils {
   bool isValidFilePath(const std::string &);
   bool fileExist(const std::string &);
   bool pathExist(const std::string &);

   bool cpFile(const std::string &from, const std::string &to);
   bool rmFile(const std::string &);

   std::string absolutePath(const std::string &);
   bool mkPath(const std::string &);
   bool rmDir(const std::string &);

   std::vector<std::string> readDir(const std::string &path
      , const std::string &filter = "", bool onlyFiles = true);
}

namespace SystemFilePaths {
   std::string appDataLocation();
   std::string configDataLocation();
   std::string applicationDirIfKnown();

   void setArgV0(const char *);
}

#endif // __SYSTEM_FILE_UTILS_H__