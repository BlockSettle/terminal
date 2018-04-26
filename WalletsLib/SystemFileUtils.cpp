#include "SystemFileUtils.h"
#include <QFile>
#include <QFileInfo>

using namespace SystemFileUtils;

bool SystemFileUtils::IsValidFilePath(const file_path_type& path)
{
   QFileInfo fi(QString::fromStdString(path));
   if (!fi.isNativePath()) {
      return false;
   }
   return true;
}

bool SystemFileUtils::FileExist(const file_path_type& path)
{
   if (!QFile::exists(QString::fromStdString(path))) {
      return false;
   }
   return true;
}

bool SystemFileUtils::SetFileFlag(const file_path_type& path)
{
// TODO: not implemented
   return false;
}

bool SystemFileUtils::ClearFileFlag(const file_path_type& path)
{
// TODO: not implemented
   return false;
}

bool SystemFileUtils::FileCopy(const file_path_type& from, const file_path_type& to)
{
// TODO: not implemented
   return false;
}

bool SystemFileUtils::RemoveFile(const file_path_type& path)
{
// TODO: not implemented
   return false;
}
