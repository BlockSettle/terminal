/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef PROCESS_CONTROL_H
#define PROCESS_CONTROL_H

#ifdef WIN32
#  include <windows.h>
#else
#endif
#include <string>
#include <vector>


class ProcessControl
{
public:
   ~ProcessControl();

   bool run(const std::string &program, const std::vector<std::string> &args);
   bool kill();

private:
#ifdef WIN32
   HANDLE hProcess_ = 0;
   HANDLE hThread_ = 0;
#else
   int   pid_ = 0;
#endif
};

#endif // PROCESS_CONTROL_H
