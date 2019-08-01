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
#include "BinaryData.h"

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

   StaticLogger::loggerPtr = spdlog::basic_logger_mt("unit_tests", "unit_tests.log");
   StaticLogger::loggerPtr->set_pattern("[%D %H:%M:%S.%e] [%l](%t): %v");
   StaticLogger::loggerPtr->set_level(spdlog::level::debug);
   StaticLogger::loggerPtr->flush_on(spdlog::level::debug);
   StaticLogger::loggerPtr->info("Started BS unit tests");

   btc_ecc_start();
   startupBIP151CTX();
   startupBIP150CTX(4, true);
   srand(time(0));

   ::testing::InitGoogleTest(&argc, argv);

   qInstallMessageHandler(loggerOutput);
   QApplication app(argc, argv);

   qRegisterMetaType<std::string>();
   qRegisterMetaType<std::vector<BinaryData>>();
   qRegisterMetaType<BinaryData>();

   //::testing::AddGlobalTestEnvironment(new TestEnv(logger));

   QTimer::singleShot(0, [] {
      rc = RUN_ALL_TESTS();
      QApplication::quit();
   });
   app.exec();
   return rc;
}
