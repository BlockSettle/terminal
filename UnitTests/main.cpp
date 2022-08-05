/*

***********************************************************************************
* Copyright (C) 2019 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifdef _MSC_VER
#  include <winsock2.h>
#endif

#include <atomic>
#include <thread>
#include <vector>

#include <QApplication>
#include <QDebug>
#include <QFileInfo>
#include <QThread>
#include <QTimer>
#include <QtPlugin>

#include <gtest/gtest.h>

#include "TestEnv.h"
#include "ArmoryConfig.h"
#include "BinaryData.h"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#ifdef WIN32
Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin)
#elif __linux__
Q_IMPORT_PLUGIN(QXcbIntegrationPlugin)
#elif __APPLE__
Q_IMPORT_PLUGIN(QCocoaIntegrationPlugin)
#endif

Q_DECLARE_METATYPE(std::string)
Q_DECLARE_METATYPE(std::vector<BinaryData>)
Q_DECLARE_METATYPE(BinaryData)

int rc = 0;

void loggerOutput(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
   QFileInfo fi(QLatin1String(context.file));
   switch (type) {
   case QtDebugMsg:
      StaticLogger::loggerPtr->debug("[{}:{}] {}", fi.fileName().toStdString(), context.line, msg.toStdString());
      break;
   case QtInfoMsg:
      StaticLogger::loggerPtr->info("[{}:{}] {}", fi.fileName().toStdString(), context.line, msg.toStdString());
      break;
   case QtWarningMsg:
      StaticLogger::loggerPtr->warn("[{}:{}] {}", fi.fileName().toStdString(), context.line, msg.toStdString());
      break;
   case QtCriticalMsg:
   case QtFatalMsg:
   default:
      StaticLogger::loggerPtr->error("[{}:{}] {}", fi.fileName().toStdString(), context.line, msg.toStdString());
      break;
   }
}

int main(int argc, char** argv)
{
#ifdef _MSC_VER
//   _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
   WSADATA wsaData;
   WORD wVersion = MAKEWORD(2, 0);
   WSAStartup(wVersion, &wsaData);
#endif

   std::vector<spdlog::sink_ptr> sinks;

   //create stdout color sink
   auto stdout_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();

   // create file sink
   auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("unit_tests.log");

   sinks.push_back(stdout_sink);
   sinks.push_back(file_sink);

   StaticLogger::loggerPtr = std::make_shared<spdlog::logger>("", begin(sinks), end(sinks));

   StaticLogger::loggerPtr->set_pattern("[%D %H:%M:%S.%e] [%l](%t) %s:%#:%!: %v");
   StaticLogger::loggerPtr->set_level(spdlog::level::debug);
   StaticLogger::loggerPtr->flush_on(spdlog::level::debug);
   StaticLogger::loggerPtr->info("Started BS unit tests");

   btc_ecc_start();
   startupBIP151CTX();
   startupBIP150CTX(4);
   srand(time(0));

   ::testing::InitGoogleTest(&argc, argv);

   qInstallMessageHandler(loggerOutput);
   QApplication app(argc, argv);

   qRegisterMetaType<std::string>();
   qRegisterMetaType<std::vector<BinaryData>>();
   qRegisterMetaType<BinaryData>();

   //::testing::AddGlobalTestEnvironment(new TestEnv(logger));
   Armory::Config::NetworkSettings::selectNetwork(Armory::Config::NETWORK_MODE_TESTNET);

   QTimer::singleShot(0, [] {
      rc = RUN_ALL_TESTS();
      QApplication::quit();
   });
   app.exec();
   return rc;
}
