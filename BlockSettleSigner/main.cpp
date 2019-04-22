#include <atomic>
#include <condition_variable>
#include <iostream>
#include <memory>
#include <mutex>
#include <signal.h>
#include <btc/ecc.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_sinks.h>
#include "HeadlessApp.h"
#include "HeadlessSettings.h"
#include "LogManager.h"
#include "SystemFileUtils.h"
#include "ZMQ_BIP15X_ServerConnection.h"

static std::mutex mainLoopMtx;
static std::condition_variable mainLoopCV;
static std::atomic_bool mainLoopRunning{ true };

static int HeadlessApp(int argc, char **argv)
{
   bs::LogManager logMgr;
   auto loggerStdout = logMgr.logger("settings");

   const auto dir = SystemFilePaths::appDataLocation();
   if (!SystemFileUtils::pathExist(dir)) {
      loggerStdout->info("Creating missing dir {}", dir);
      if (!SystemFileUtils::mkPath(dir)) {
         loggerStdout->error("Failed to create path {} - exitting", dir);
         exit(1);
      }
   }

   const auto settings = std::make_shared<HeadlessSettings>(loggerStdout);
   if (!settings->loadSettings(argc, argv)) {
      loggerStdout->error("Failed to load settings");
      return EXIT_FAILURE;
   }
   auto logger = loggerStdout;
   if (!settings->logFile().empty()) {
      logMgr.add(bs::LogConfig{ settings->logFile(), "%D %H:%M:%S.%e (%t)[%L]: %v", "" });
      logger = logMgr.logger();
   }

#ifndef NDEBUG
#ifdef WIN32
   // set zero buffer for stdout and stderr
   setvbuf(stdout, NULL, _IONBF, 0 );
   setvbuf(stderr, NULL, _IONBF, 0 );
#endif
#endif

   logger->info("Starting BS Signer...");
#ifdef NDEBUG
   try {
#endif
      HeadlessAppObj appObj(logger, settings);
      appObj.start();

#ifdef WIN32
      while (mainLoopRunning) {
         MSG msg;
         BOOL bRet = GetMessage(&msg, NULL, 0, 0 );
         logger->info("GetMessage ret:{}, msg:{}", bRet, msg.message);

         if (bRet == -1) {
            // handle the error and exit
            break;
         }
         else if (bRet == 0) {
            // handle normal exit
            // WM_QUIT message force GetMessage to return 0
            break;
         }
         else {
            // normally no events come here since app has no any window.
            // No need to run TranslateMessage and DispatchMessage
            // TranslateMessage(&msg);
            // DispatchMessage(&msg);

            if (msg.message == WM_CLOSE) {
               break;
            }
         }
      }
#else
      while (mainLoopRunning) {
         std::unique_lock<std::mutex> lock(mainLoopMtx);
         mainLoopCV.wait_for(lock, std::chrono::seconds{ 1 });
      }
#endif // WIN32

#ifdef NDEBUG
   }
   catch (const std::exception &e) {
      std::string errMsg = "Failed to start headless process: ";
      errMsg.append(e.what());
      logger->error("{}", errMsg);
      std::cerr << errMsg << std::endl;
      return 1;
   }
#endif
   return 0;
}

#ifndef WIN32
void sigHandler(int signum, siginfo_t *, void *)
{
   mainLoopRunning = false;
   mainLoopCV.notify_one();
}
#endif   // WIN32

int main(int argc, char** argv)
{
#ifndef WIN32
   struct sigaction act;
   memset(&act, 0, sizeof(act));
   act.sa_sigaction = sigHandler;
   act.sa_flags = SA_SIGINFO;

   sigaction(SIGINT, &act, NULL);
   sigaction(SIGTERM, &act, NULL);
#endif

   SystemFilePaths::setArgV0(argv[0]);

   srand(std::time(nullptr));

   // Initialize libbtc, BIP 150, and BIP 151. 150 uses the proprietary "public"
   // Armory setting designed to allow the ArmoryDB server to not have to verify
   // clients. Prevents us from having to import tons of keys into the server.
   btc_ecc_start();
   startupBIP151CTX();
   startupBIP150CTX(4, true);

   return HeadlessApp(argc, argv);
}
