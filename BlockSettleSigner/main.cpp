#include <memory>
#include <iostream>
#include <btc/ecc.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_sinks.h>
#include "HeadlessApp.h"
#include "HeadlessSettings.h"
#include "LogManager.h"
#include "ZMQ_BIP15X_ServerConnection.h"


static int HeadlessApp(int argc, char **argv)
{
   bs::LogManager logMgr;
   auto loggerStdout = logMgr.logger("settings");

/*   QDir dir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
   if (!dir.exists()) {
      loggerStdout->info("Creating missing dir {}", dir.path().toStdString());
      dir.mkpath(dir.path());
   }*/

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
   try {
      HeadlessAppObj appObj(logger, settings);
      appObj.start();

      while (true) {
         std::this_thread::sleep_for(std::chrono::seconds(1));
      }
   }
   catch (const std::exception &e) {
      std::string errMsg = "Failed to start headless process: ";
      errMsg.append(e.what());
      logger->error("{}", errMsg);
      std::cerr << errMsg << std::endl;
      return 1;
   }
   return 0;
}

int main(int argc, char** argv)
{
   // Initialize libbtc, BIP 150, and BIP 151. 150 uses the proprietary "public"
   // Armory setting designed to allow the ArmoryDB server to not have to verify
   // clients. Prevents us from having to import tons of keys into the server.
   btc_ecc_start();
   startupBIP151CTX();
   startupBIP150CTX(4, true);

   return HeadlessApp(argc, argv);
}
