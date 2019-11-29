/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "ProcessControl.h"
#ifndef WIN32
#  include <unistd.h>
#  include <signal.h>
#endif
#include <string.h>


ProcessControl::~ProcessControl()
{
   kill();
}

bool ProcessControl::kill()
{
#ifdef WIN32
   if (hProcess_) {
      TerminateProcess(hProcess_, 0);
      CloseHandle(hProcess_);
      CloseHandle(hThread_);
   } else {
      return false;
   }
#else
   if (pid_ > 0) {
      const auto rc = ::kill(pid_, SIGINT);
      pid_ = 0;
      return (rc == 0);
   }
   else {
      return false;
   }
#endif
   return true;
}

bool ProcessControl::run(const std::string &program, const std::vector<std::string> &args)
{
#ifdef WIN32
   std::string params;
   for (const auto &arg : args) {
      if (!params.empty()) {
         params += " ";
      }
      params += arg;
   }
   char cmdLine[MAX_PATH];
   memcpy(cmdLine, (program + " " + params).c_str()
      , params.size() + program.size() + 1);
   cmdLine[params.size() + program.size() + 1] = 0;
   STARTUPINFO si;
   PROCESS_INFORMATION pi;
   ZeroMemory(&si, sizeof(si));
   si.cb = sizeof(si);
   ZeroMemory(&pi, sizeof(pi));

   if (!CreateProcess(program.c_str(), cmdLine, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
      return false;
   }
   hProcess_ = pi.hProcess;
   hThread_ = pi.hThread;
#else
   pid_ = fork();
   if (pid_ == -1) {
      return false;
   }
   if (pid_ == 0) {  //we're in child process, execute program here
      const size_t argvSize = args.size() + 2;   // add program name and trailing NULL
      char **argvList = (char **)malloc(sizeof(char *) * argvSize);
      size_t argvIdx = 0;
      argvList[argvIdx++] = strdup(program.c_str());
      for (const auto &arg : args) {
         argvList[argvIdx++] = strdup(arg.c_str());
      }
      argvList[argvIdx] = nullptr;
      // Use execvp to be able search for the binary in PATH if abs path is not known
      execvp(program.c_str(), argvList);
   }
#endif   //WIN32
   return true;
}
