#include <QApplication>
#include <QIcon>
#include <QStyleFactory>
#include <QtPlugin>
#include <QtGui/QGuiApplication>
#include <QtQml/QQmlApplicationEngine>
#include <QtQuickControls2/QQuickStyle>
#include <QSplashScreen>
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
#include "ArmoryConnection.h"
#include "HeadlessApp.h"
#include "SignerAdapter.h"
#include "SignerSettings.h"
#include "QMLApp.h"
#include "ZMQHelperFunctions.h"
#include "zmq.h"

Q_DECLARE_METATYPE(std::string)
Q_DECLARE_METATYPE(std::vector<BinaryData>)
Q_DECLARE_METATYPE(BinaryData)

// Generate a random CurveZMQ keypair and write the keys to files.
// IN:  Logger (std::shared_ptr<spdlog::logger>)
//      Path where to check/write public key file (const QString&)
//      Path where to check/write private key file (const QString&)
// OUT: None
// RET: True is success, false if not
bool generateCurveZMQKeyPairFiles(std::shared_ptr<spdlog::logger> inLogger
   , const QString& pubFilePath, const QString& prvFilePath) {
   // Generate the keys.
   std::pair<SecureBinaryData, SecureBinaryData> inKeyPair;
   if (bs::network::getCurveZMQKeyPair(inKeyPair) != 0) {
      if (inLogger) {
         inLogger->error("[{}] Failure to generate CurveZMQ data - Error = {}"
            , __func__, zmq_strerror(zmq_errno()));
      }
      return false;
   }

   // Write the files. We'll overwrite anything already present (necessary when
   // either pub/prv key file is missing, so that the keys match).
   QFile pubFile(pubFilePath);
   if (!pubFile.open(QIODevice::WriteOnly)) {
      if (inLogger) {
         inLogger->error("[{}] Failure to open CurveZMQ public file ({})"
            ,__func__, pubFilePath.toStdString());
      }
      return false;
   }
   if (pubFile.write(inKeyPair.first.toBinStr().c_str()
      , inKeyPair.first.toBinStr().length()) != CURVEZMQPUBKEYBUFFERSIZE) {
      if (inLogger) {
         inLogger->error("[{}] Failure to properly write to CurveZMQ public "
            "file ({})", __func__, pubFilePath.toStdString());
      }
      pubFile.close();
      return false;
   }
   pubFile.close();

   // Limit permissions for the private file. It should only be accessible by
   // the current account, and nobody else.
   QFile prvFile(prvFilePath);
   if (!prvFile.open(QIODevice::WriteOnly)) {
      if (inLogger) {
         inLogger->error("[{}] Failure to open CurveZMQ private file ({})"
            , __func__, prvFilePath.toStdString());
      }
      return false;
   }
   if (!QFile::setPermissions(prvFilePath
         , QFileDevice::WriteOwner | QFileDevice::ReadOwner)) {
      if (inLogger) {
         inLogger->error("[{}] Failure to open CurveZMQ private file ({})"
            , __func__, prvFilePath.toStdString());
      }
      prvFile.close();
      return false;
   }
   if (prvFile.write(inKeyPair.second.toBinStr().c_str()
      , inKeyPair.second.toBinStr().length()) != CURVEZMQPRVKEYBUFFERSIZE) {
      if (inLogger) {
         inLogger->error("[{}] Failure to properly write to CurveZMQ private "
            "file ({})", __func__, prvFilePath.toStdString());
      }
      prvFile.close();
      return false;
   }
   prvFile.close();

   if (inLogger) {
      inLogger->info("[{}] CurveZMQ files written.", __func__);
      inLogger->info("[{}] Public key file - {}", pubFilePath.toStdString());
      inLogger->info("[{}] Private key file - {}", prvFilePath.toStdString());
   }

   return true;
}

// Function that builds ZMQ connection pub/prv files (CurveZMQ) if the files
// don't already exist.
// IN:  Signer command line settings (std::shared_ptr<SignerSettings>)
//      Logger (std::shared_ptr<spdlog::logger>)
// OUT: None
// RET: True is success, false if not
bool buildZMQConnFiles(std::shared_ptr<SignerSettings> inSettings
   , std::shared_ptr<spdlog::logger> inLogger) {
   QFileInfo pubFileInfo(inSettings->zmqPubKeyFile());
   QFileInfo prvFileInfo(inSettings->zmqPrvKeyFile());
   if (inLogger) {
      inLogger->info("[{}] Loading ZMQ connection keypair files."
         , __func__);
      inLogger->info("[{}] ZMQ connection public key file - {}"
         , __func__, inSettings->zmqPubKeyFile().toStdString());
      inLogger->info("[{}] ZMQ connection private key file - {}"
         , __func__, inSettings->zmqPrvKeyFile().toStdString());
   }

   // Create new files if they don't exist, and create the absolute directories
   // if they don't exist.
   if (!pubFileInfo.exists() || pubFileInfo.isDir()
      || !prvFileInfo.exists() || prvFileInfo.isDir()) {
      if (inLogger) {
         inLogger->info("[{}] ZMQ connection keypair doesn't exist. "
            "Generating new keypair.", __func__);
      }

      if (!QDir().mkpath(pubFileInfo.absolutePath())) {
         if (inLogger) {
            inLogger->info("[{}] Unable to create ZMQ connection public key "
               "directory ({})", __func__
               , pubFileInfo.absolutePath().toStdString());
         }
      }
      if (!QDir().mkpath(prvFileInfo.absolutePath())) {
         if (inLogger) {
            inLogger->info("[{}] Unable to create ZMQ connection private key "
               "directory ({})", __func__
               , prvFileInfo.absolutePath().toStdString());
         }
      }

      if (!generateCurveZMQKeyPairFiles(inLogger, pubFileInfo.absoluteFilePath()
         , prvFileInfo.absoluteFilePath())) {
         return false;
      }
   }

   return true;
}

static int HeadlessApp(int argc, char **argv)
{
   QCoreApplication app(argc, argv);
   app.setApplicationName(QLatin1String("blocksettle"));
   app.setOrganizationDomain(QLatin1String("blocksettle.com"));
   app.setOrganizationName(QLatin1String("blocksettle"));

   const auto settings = std::make_shared<SignerSettings>(app.arguments());
   auto logger = spdlog::basic_logger_mt("app_logger"
      , settings->logFileName().toStdString());
   // [date time.miliseconds] [level](thread id): text
   logger->set_pattern("%D %H:%M:%S.%e (%t)[%L]: %v");
   logger->set_level(spdlog::level::debug);
   logger->flush_on(spdlog::level::debug);

   logger->info("Starting BS Signer...");
   try {
      // Go ahead and build the ZMQ connection encryption files, even if
      // they're not used.
      if (!buildZMQConnFiles(settings, logger)) {
         if (logger) {
            logger->info("[{}] ZMQ connection keypair files could not be "
               "generated. The ZMQ connection can not be created."
               , __func__);
         }
      }

      HeadlessAppObj appObj(logger, settings);
      QObject::connect(&appObj, &HeadlessAppObj::finished, &app
                       , &QCoreApplication::quit);
      QTimer::singleShot(0, &appObj, &HeadlessAppObj::Start);

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

#if defined (Q_OS_WIN)
Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin)
Q_IMPORT_PLUGIN(QWindowsPrinterSupportPlugin)
#elif defined (Q_OS_MAC)
Q_IMPORT_PLUGIN(QCocoaIntegrationPlugin)
Q_IMPORT_PLUGIN(QCocoaPrinterSupportPlugin)
#elif defined (Q_OS_LINUX)
Q_IMPORT_PLUGIN(QXcbIntegrationPlugin)
Q_IMPORT_PLUGIN(QtQuick2PrivateWidgetsPlugin)
Q_IMPORT_PLUGIN(QCupsPrinterSupportPlugin)
#endif

Q_IMPORT_PLUGIN(QICOPlugin)

Q_IMPORT_PLUGIN(QtQuick2Plugin)
Q_IMPORT_PLUGIN(QtQuick2DialogsPlugin)
Q_IMPORT_PLUGIN(QtQuick2DialogsPrivatePlugin)
Q_IMPORT_PLUGIN(QtQuick2WindowPlugin)
Q_IMPORT_PLUGIN(QtQuickControls2Plugin)
Q_IMPORT_PLUGIN(QtQuickTemplates2Plugin)
Q_IMPORT_PLUGIN(QtQuickControls1Plugin)
Q_IMPORT_PLUGIN(QtQuickLayoutsPlugin)
Q_IMPORT_PLUGIN(QtQmlModelsPlugin)
Q_IMPORT_PLUGIN(QmlFolderListModelPlugin)
Q_IMPORT_PLUGIN(QmlSettingsPlugin)

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

static int QMLApp(int argc, char **argv)
{
   QGuiApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
   QApplication app(argc, argv);
   app.setApplicationName(QLatin1String("blocksettle"));
   app.setOrganizationDomain(QLatin1String("blocksettle.com"));
   app.setOrganizationName(QLatin1String("blocksettle"));
   app.setWindowIcon(QIcon(QStringLiteral(":/images/bs_logo.png")));

   // Initialize libbtc, BIP 150, and BIP 151. 150 uses the proprietary "public"
   // Armory setting designed to allow the ArmoryDB server to not have to verify
   // clients. Prevents us from having to import tons of keys into the server.
   btc_ecc_start();
   startupBIP151CTX();
   startupBIP150CTX(4, true);

   // ToDo: support 2.0 styles
   // app.setStyle(QStyleFactory::create(QStringLiteral("Universal")));

   // we need this only for desktop app
   const auto splashImage = QPixmap(QLatin1String(":/FULL_LOGO")).scaledToWidth(390, Qt::SmoothTransformation);
   QSplashScreen splashScreen(splashImage);
   splashScreen.setWindowFlag(Qt::WindowStaysOnTopHint);
   splashScreen.show();

   const auto settings = std::make_shared<SignerSettings>(app.arguments());
   std::shared_ptr<spdlog::logger> logger;
   try {
      logger = spdlog::basic_logger_mt("app_logger"
         , settings->logFileName().toStdString());
      // [date time.miliseconds] [level](thread id): text
      logger->set_pattern("%D %H:%M:%S.%e [%L](%t): %v");
      logger->set_level(spdlog::level::debug);
      logger->flush_on(spdlog::level::debug);
   }
   catch (const spdlog::spdlog_ex &e) {
      std::cerr << "Failed to create logger in "
         << settings->logFileName().toStdString() << ": " << e.what()
         << " - logging to console" << std::endl;
      logger = spdlog::stdout_logger_mt("app_logger");
      logger->set_pattern("[%L](%t): %v");
      logger->set_level(spdlog::level::debug);
      logger->flush_on(spdlog::level::debug);
   }

   // Go ahead and build the headless connection encryption files, even if we
   // don't use them. If they already exist, we'll leave them alone.
   logger->info("Starting BS Signer...");
   if (!buildZMQConnFiles(settings, logger)) {
      if (logger) {
         logger->info("[{}] ZMQ connection keypair files could not be "
            "generated. The ZMQ connection can not be created."
            , __func__);
      }
   }

   try {
      HeadlessAppObj appObj(logger, settings);
      SignerAdapter adapter(logger, &appObj);
      QObject::connect(&appObj, &HeadlessAppObj::finished, &app
         , &QCoreApplication::quit);
      QTimer::singleShot(0, &appObj, &HeadlessAppObj::Start);

      QQmlApplicationEngine engine;
      QMLAppObj qmlAppObj(&adapter, logger, settings, engine.rootContext());
      QObject::connect(&qmlAppObj, &QMLAppObj::loadingComplete, &splashScreen
         , &QSplashScreen::close);
      QTimer::singleShot(0, &qmlAppObj, &QMLAppObj::Start);

      engine.load(QUrl(QStringLiteral("qrc:/qml/main.qml")));
      if (engine.rootObjects().isEmpty()) {
         throw std::runtime_error("Failed to load main QML file");
      }
      qmlAppObj.SetRootObject(engine.rootObjects().at(0));
      return app.exec();
   }
   catch (const std::exception &e) {
      logger->critical("Failed to start signer: {}", e.what());
      std::cerr << "Failed to start signer:" << e.what() << std::endl;
      return -1;
   }
}

bool isHeadlessMode(int argc, char** argv)
{
   for (int i = 1; i < argc; i++) {
      if (!strcmp(argv[i], "--headless")) {
         return true;
      }
   }
   return false;
}

int main(int argc, char** argv)
{
   qRegisterMetaType<std::string>();
   qRegisterMetaType<std::vector<BinaryData>>();
   qRegisterMetaType<BinaryData>();

   btc_ecc_start(); // Initialize libbtc.

   if (isHeadlessMode(argc, argv)) {
      return HeadlessApp(argc, argv);
   }
   else {
      return QMLApp(argc, argv);
   }
}
