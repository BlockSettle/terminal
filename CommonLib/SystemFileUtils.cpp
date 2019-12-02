/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "SystemFileUtils.h"
#ifdef WIN32
#  include <ShlObj.h>
#  include <KnownFolders.h>
#  include <direct.h>
#  include <dirent_win32.h>
#  include <Shlwapi.h>
#else
#  include <stdlib.h>
#  include <dirent.h>
#  include <glob.h>
#  include <unistd.h>
#endif
#include <algorithm>
#include <string.h>
#include <sys/stat.h>
#include <fstream>

namespace {
   const std::string kAppDir = "blocksettle";
#ifndef WIN32
#ifdef __APPLE__
   const std::string kDataDir = "Library/Application Support";
   const std::string kConfigDir = "Library/Application Support";
#else
   const std::string kDataDir = ".local/share";
   const std::string kConfigDir = ".config";
#endif
#endif

   std::string g_appDir;
} // namespace

using namespace SystemFileUtils;

bool SystemFileUtils::isValidFilePath(const std::string &path)
{
   // TODO: not implemented - shouldn't rely on path existence in FS
   return true;
}

bool SystemFileUtils::fileExist(const std::string &path)
{
   struct stat buf;
   if (::stat(path.c_str(), &buf) != 0) {
      return false;
   }
   return ((buf.st_mode & S_IFMT) == S_IFREG);
}

bool SystemFileUtils::pathExist(const std::string &path)
{
   struct stat buf;
   if (::stat(path.c_str(), &buf) != 0) {
      return false;
   }
   return ((buf.st_mode & S_IFMT) == S_IFDIR);
}

bool SystemFileUtils::cpFile(const std::string &from, const std::string &to)
{
   std::ifstream src(from);
   if (!src.good()) {
      return false;
   }
   std::ofstream dst(to);
   if (!dst.good()) {
      return false;
   }
   dst << src.rdbuf();
   return true;
}

bool SystemFileUtils::rmFile(const std::string& path)
{
   return (std::remove(path.c_str()) == 0);
}

std::string SystemFileUtils::absolutePath(const std::string &path)
{
#ifdef WIN32
   char fullPath[MAX_PATH];
   if (::GetFullPathName(path.c_str(), MAX_PATH, fullPath, nullptr) <= 0) {
      return path;
   }
   return std::string(fullPath);
#else
   //TODO: use std::filesystem::absolute from C++17 when we're ready to migrate
#endif
   return path;
}

bool SystemFileUtils::mkPath(const std::string &path)
{
   std::vector<std::string> dirs = { path };
   auto p = path;
   while (!p.empty()) {
      const auto pSep = p.find_last_of('/');
      if (pSep == std::string::npos) {
         break;
      }
      p = p.substr(0, pSep);
      if (!p.empty()) {
         dirs.push_back(p);
      }
   }
   std::reverse(dirs.begin(), dirs.end());

   for (const auto &dir : dirs) {
      if (pathExist(dir)) {
         continue;
      }
#ifdef WIN32
      if ((dir.size() == 2) && (dir[1] == ':')) {
         continue;
      }
      if (_mkdir(dir.c_str()) != 0) {
         return false;
      }
#else
      if (mkdir(dir.c_str(), 0755) != 0) {
         return false;
      }
#endif
   }
   return true;
}

std::vector<std::string> SystemFileUtils::readDir(const std::string &path
   , const std::string &filter, bool onlyFiles)
{
   if (!pathExist(path)) {
      return {};
   }
   std::vector<std::string> result;
#ifdef WIN32
   auto d = ::opendir(path.c_str());
   if (!d) {
      return {};
   }
   struct dirent *ent;
   while (ent = ::readdir(d)) {
      const std::string entry = ent->d_name;
      if ((entry == ".") || (entry == "..")) {
         continue;
      }
      if (onlyFiles && pathExist(path + "/" + entry)) {
         continue;
      }
      if (!filter.empty() && !::PathMatchSpec(entry.c_str(), filter.c_str())) {
         continue;
      }
      result.emplace_back(std::move(entry));
   }
   closedir(d);
#else
   glob_t globResult;
   memset(&globResult, 0, sizeof(globResult));
   const auto fltCopy = filter.empty() ? std::string("*") : filter;
   if (glob((path + "/" + fltCopy).c_str(), 0, NULL, &globResult) != 0) {
      globfree(&globResult);
      return {};
   }
   for (unsigned int i = 0; i < globResult.gl_pathc; ++i) {
      std::string entry = globResult.gl_pathv[i];
      if ((entry == ".") || (entry == "..")) {
         continue;
      }
      if (onlyFiles && pathExist(entry)) {
         continue;
      }
      entry.erase(0, path.size() + 1);
      result.emplace_back(std::move(entry));
   }
   globfree(&globResult);
#endif
   return result;
}

bool SystemFileUtils::rmDir(const std::string &path)
{
   const auto files = readDir(path);
   bool result = true;
   for (const auto &file : files) {
      result &= rmFile(path + "/" + file);
   }
   if (result) {
      result &= (rmdir(path.c_str()) == 0);
   }
   return result;
}


std::string SystemFilePaths::appDataLocation()
{
   std::string result;
#ifdef WIN32
   PWSTR path = nullptr;
   char strPath[512];
   SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, NULL, &path);
   wcstombs(strPath, path, sizeof(strPath));
   result = strPath;
   ::CoTaskMemFree(static_cast<void*>(path));
   std::replace(result.begin(), result.end(), '\\', '/');
#else
   result = getenv("HOME");
   result += "/" + kDataDir;
#endif   //WIN32
   result += "/" + kAppDir;
   return result;
}

std::string SystemFilePaths::configDataLocation()
{
#ifdef WIN32
   return appDataLocation();
#else
   std::string result = getenv("HOME");
   result += "/" + kConfigDir + "/" + kAppDir;
   return result;
#endif
}

std::string SystemFilePaths::applicationDirIfKnown()
{
   return g_appDir;
}

void SystemFilePaths::setArgV0(const char *arg)
{
   g_appDir = arg;
#ifdef WIN32
   std::replace(g_appDir.begin(), g_appDir.end(), '\\', '/');
#endif
   const auto sepPos = g_appDir.find_last_of('/');
   if (sepPos != std::string::npos) {
      g_appDir = g_appDir.substr(0, sepPos);
   } else {
      // If binary is started with only basename (when found from $PATH) appDir is not easily known
      g_appDir.clear();
   }
}
