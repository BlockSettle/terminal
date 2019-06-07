#include <atomic>
#include <condition_variable>
#include <iostream>
#include <memory>
#include <mutex>
#include <csignal>
#include <btc/ecc.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_sinks.h>
#include "DispatchQueue.h"
#include "HeadlessApp.h"
#include "HeadlessSettings.h"
#include "LogManager.h"
#include "SystemFileUtils.h"
#include "ZMQ_BIP15X_ServerConnection.h"

namespace {

   volatile std::sig_atomic_t g_signalStatus;

} //namespace

static int process(const std::shared_ptr<spdlog::logger> &logger
   , const std::shared_ptr<HeadlessSettings> &settings)
{
   auto queue = std::make_shared<DispatchQueue>();

   HeadlessAppObj appObj(logger, settings, queue);

   appObj.start();

   while (!queue->done()) {
      queue->tryProcess(std::chrono::seconds(1));

      if (g_signalStatus != 0) {
         logger->info("quit signal received, shutdown...");
         queue->quit();
         g_signalStatus = 0;
      }
   }

   // Stop all background processing just in case
   appObj.stop();

   return EXIT_SUCCESS;
}

static int processChecked(const std::shared_ptr<spdlog::logger> &logger
   , const std::shared_ptr<HeadlessSettings> &settings)
{
   try {
      return process(logger, settings);
   }
   catch (const std::exception &e) {
      std::string errMsg = "Failed to start headless process: ";
      errMsg.append(e.what());
      logger->error("{}", errMsg);
      std::cerr << errMsg << std::endl;
      return EXIT_FAILURE;
   }
}

static int HeadlessApp(int argc, char **argv)
{
   bs::LogManager logMgr;
   auto loggerStdout = logMgr.logger("settings");

   const auto dir = SystemFilePaths::appDataLocation();
   if (!SystemFileUtils::pathExist(dir)) {
      loggerStdout->info("Creating missing dir {}", dir);
      if (!SystemFileUtils::mkPath(dir)) {
         loggerStdout->error("Failed to create path {} - exitting", dir);
         return EXIT_FAILURE;
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

   logger->info("Starting BS Signer...");

#ifdef NDEBUG
   return processChecked(logger, settings);
#else
   return process(logger, settings);
#endif
}

void sigHandler(int signal)
{
   g_signalStatus = signal;
}

int main(int argc, char** argv)
{
   g_signalStatus = 0;
#ifndef WIN32
   std::signal(SIGINT, sigHandler);
   std::signal(SIGTERM, sigHandler);
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
