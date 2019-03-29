#include <QCoreApplication>
#include <QTimer>
#include <QFileInfo>
#include <QStandardPaths>
#include <QDir>
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

Q_DECLARE_METATYPE(std::string)
Q_DECLARE_METATYPE(std::vector<BinaryData>)
Q_DECLARE_METATYPE(BinaryData)

static int HeadlessApp(int argc, char **argv)
{
   QCoreApplication app(argc, argv);
   app.setApplicationName(QLatin1String("blocksettle"));
   app.setOrganizationDomain(QLatin1String("blocksettle.com"));
   app.setOrganizationName(QLatin1String("BlockSettle"));

   bs::LogManager logMgr;
   auto loggerStdout = logMgr.logger("settings");

   const auto settings = std::make_shared<HeadlessSettings>(loggerStdout);
   if (!settings->loadSettings(app.arguments())) {
      loggerStdout->error("Failed to load settings");
      return EXIT_FAILURE;
   }
   auto logger = loggerStdout;
   if (!settings->logFile().empty()) {
      logMgr.add(bs::LogConfig{ settings->logFile(), "%D %H:%M:%S.%e (%t)[%L]: %v", "" });
      logger = logMgr.logger();
   }

   logger->info("Starting BS Signer...");
   try {
      HeadlessAppObj appObj(logger, settings);
      QObject::connect(&appObj, &HeadlessAppObj::finished, &app
                       , &QCoreApplication::quit);
      QTimer::singleShot(0, &appObj, &HeadlessAppObj::start);

      return app.exec();
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

/*class SignerApplication : public QApplication
{
public:
   SignerApplication(int argc, char **argv) : QGuiApplication(argc, argv) {
      setApplicationName(QLatin1String("blocksettle"));
      setOrganizationDomain(QLatin1String("blocksettle.com"));
      setOrganizationName(QLatin1String("blocksettle"));
      setWindowIcon(QIcon(QStringLiteral(":/images/bs_logo.png")));
   }

   bool notify(QObject *receiver, QEvent *e) override {
      try {
         return QGuiApplication::notify(receiver, e);
      }
      catch (const std::exception &e) {
      }
      return false;
   }
};*/

int main(int argc, char** argv)
{
   qRegisterMetaType<std::string>();
   qRegisterMetaType<std::vector<BinaryData>>();
   qRegisterMetaType<BinaryData>();

   // Initialize libbtc, BIP 150, and BIP 151. 150 uses the proprietary "public"
   // Armory setting designed to allow the ArmoryDB server to not have to verify
   // clients. Prevents us from having to import tons of keys into the server.
   btc_ecc_start();
   startupBIP151CTX();
   startupBIP150CTX(4, true);

   return HeadlessApp(argc, argv);
}
