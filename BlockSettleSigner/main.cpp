#include <btc/ecc.h>
#include <iostream>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_sinks.h>

#include "BIP150_151.h"
#include "DispatchQueue.h"
#include "HeadlessApp.h"
#include "HeadlessSettings.h"
#include "LogManager.h"
#include "SignalsHandler.h"
#include "SystemFileUtils.h"

static int process(const std::shared_ptr<spdlog::logger> &logger
   , const std::shared_ptr<HeadlessSettings> &settings)
{
   auto queue = std::make_shared<DispatchQueue>();

   SignalsHandler::registerHandler([queue, logger](int signal) {
      logger->info("quit signal received, shutdown...");
      queue->quit();
   });

   HeadlessAppObj appObj(logger, settings, queue);

   appObj.start();

   while (!queue->done()) {
      queue->tryProcess();
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
         loggerStdout->error("Failed to create path {} - exiting", dir);
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
      bs::LogConfig config;
      config.fileName = settings->logFile();
      config.pattern = "%D %H:%M:%S.%e (%t)[%L]: %v";

#ifdef NDEBUG
      config.level = bs::LogLevel::err;
#else
      config.level = bs::LogLevel::debug;
#endif

      logMgr.add(config);
      logger = logMgr.logger();
   }

   // Enable terminal key checks if two way auth is enabled (or litegui is used).
   // This also affects GUI connection because the flag works globally for now.
   // So if remote signer has two-way auth disabled GUI connection will be less secure too.
   const bool twoWayEnabled = settings->twoWaySignerAuth() || (settings->runMode() == bs::signer::RunMode::litegui);
   startupBIP151CTX();
   startupBIP150CTX(4, !twoWayEnabled);

   logger->info("Starting BS Signer...");

#ifdef NDEBUG
   return processChecked(logger, settings);
#else
   return process(logger, settings);
#endif
}

int main(int argc, char** argv)
{
   SystemFilePaths::setArgV0(argv[0]);

   srand(std::time(nullptr));

   // Initialize libbtc, BIP 150, and BIP 151. 150 uses the proprietary "public"
   // Armory setting designed to allow the ArmoryDB server to not have to verify
   // clients. Prevents us from having to import tons of keys into the server.
   btc_ecc_start();

   return HeadlessApp(argc, argv);
}
