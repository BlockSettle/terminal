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
