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
#include <QtPlugin>
#include <gtest/gtest.h>
#include <spdlog/spdlog.h>
#include "TestEnv.h"
#include "BinaryData.h"
#include "PyBlockDataManager.h"

using namespace std;
#include "LedgerEntryData.h"

#ifdef WIN32
Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin)
#elif __linux__
Q_IMPORT_PLUGIN(QXcbIntegrationPlugin)
#elif __APPLE__
Q_IMPORT_PLUGIN(QCocoaIntegrationPlugin)
#endif

Q_DECLARE_METATYPE(std::string)
Q_DECLARE_METATYPE(LedgerEntryData)
Q_DECLARE_METATYPE(std::vector<LedgerEntryData>)
Q_DECLARE_METATYPE(BinaryDataVector)
Q_DECLARE_METATYPE(BinaryData)

int rc = 0;
std::shared_ptr<spdlog::logger> logger;

void loggerOutput(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
   QFileInfo fi(QLatin1String(context.file));
   switch (type) {
   case QtDebugMsg:
      logger->debug("[{}:{}] {}", fi.fileName().toStdString(), context.line, msg.toStdString());
      break;
   case QtInfoMsg:
      logger->info("[{}:{}] {}", fi.fileName().toStdString(), context.line, msg.toStdString());
      break;
   case QtWarningMsg:
      logger->warn("[{}:{}] {}", fi.fileName().toStdString(), context.line, msg.toStdString());
      break;
   case QtCriticalMsg:
   case QtFatalMsg:
   default:
      logger->error("[{}:{}] {}", fi.fileName().toStdString(), context.line, msg.toStdString());
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

   logger = spdlog::basic_logger_mt("unit_tests", "unit_tests.log");
   logger->set_pattern("[%D %H:%M:%S.%e] [%l](%t): %v");
   logger->set_level(spdlog::level::debug);
   logger->flush_on(spdlog::level::debug);
   logger->info("Started BS unit tests");
      
   ::testing::InitGoogleTest(&argc, argv);

   qInstallMessageHandler(loggerOutput);
   QApplication app(argc, argv);

   qRegisterMetaType<std::string>();
   qRegisterMetaType<LedgerEntryData>();
   qRegisterMetaType< std::vector<LedgerEntryData> >();
   qRegisterMetaType<PyBlockDataManagerState>();
   qRegisterMetaType<BinaryDataVector>();
   qRegisterMetaType<BinaryData>();

   ::testing::AddGlobalTestEnvironment(new TestEnv(logger));

   QTimer::singleShot(0, [] {
      rc = RUN_ALL_TESTS();
      QApplication::quit();
   });
   app.exec();
   return rc;
}
