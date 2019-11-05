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
#include <QFontDatabase>
#include <QQmlContext>
#include <QQuickWindow>
#include <QtPlatformHeaders/QWindowsWindowFunctions>

#include <btc/ecc.h>
#include <iostream>
#include <thread>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_sinks.h>

#include "BIP150_151.h"
#include "DispatchQueue.h"
#include "HeadlessApp.h"
#include "HeadlessSettings.h"
#include "LogManager.h"
#include "SignalsHandler.h"
#include "SignerAdapter.h"
#include "SignerSettings.h"
#include "SystemFileUtils.h"

#include "QMLApp.h"
#include "QmlBridge.h"

#include "AppNap.h"

Q_DECLARE_METATYPE(std::string)
Q_DECLARE_METATYPE(std::vector<BinaryData>)
Q_DECLARE_METATYPE(BinaryData)
Q_DECLARE_METATYPE(SecureBinaryData)

#ifdef USE_QWindowsIntegrationPlugin
Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin)
Q_IMPORT_PLUGIN(QWindowsPrinterSupportPlugin)
#endif // USE_QWindowsIntegrationPlugin

#ifdef USE_QCocoaIntegrationPlugin
Q_IMPORT_PLUGIN(QCocoaIntegrationPlugin)
Q_IMPORT_PLUGIN(QCocoaPrinterSupportPlugin)
#endif // USE_QCocoaIntegrationPlugin

#ifdef USE_QXcbIntegrationPlugin
Q_IMPORT_PLUGIN(QXcbIntegrationPlugin)
Q_IMPORT_PLUGIN(QCupsPrinterSupportPlugin)
#endif // USE_QXcbIntegrationPlugin

#ifdef STATIC_BUILD
#if defined (Q_OS_LINUX)
Q_IMPORT_PLUGIN(QtQuick2PrivateWidgetsPlugin)
#endif

Q_IMPORT_PLUGIN(QICOPlugin)
Q_IMPORT_PLUGIN(QtQuick2Plugin)
Q_IMPORT_PLUGIN(QtQuick2WindowPlugin)
Q_IMPORT_PLUGIN(QtQuickControls2Plugin)
Q_IMPORT_PLUGIN(QtQuickTemplates2Plugin)
Q_IMPORT_PLUGIN(QtQuickControls1Plugin)
Q_IMPORT_PLUGIN(QtQuickLayoutsPlugin)
Q_IMPORT_PLUGIN(QtQmlModelsPlugin)
Q_IMPORT_PLUGIN(QmlFolderListModelPlugin)
Q_IMPORT_PLUGIN(QmlSettingsPlugin)
Q_IMPORT_PLUGIN(QtLabsPlatformPlugin)

#endif // STATIC_BUILD

namespace bs {
   namespace signer {
      class Queue {
      public:
         Queue(const std::shared_ptr<spdlog::logger> &logger
            , const std::shared_ptr<HeadlessSettings> &settings)
            : logger_(logger), queue_(std::make_shared<DispatchQueue>())
            , appObj_(logger, settings, queue_)
         {
            SignalsHandler::registerHandler([this](int signal) {
               logger_->info("quit signal received, shutdown...");
               queue_->quit();
            });

            appObj_.start();

            thrProc_ = std::thread([this] {
               logger_->debug("processing thread started");
#ifdef NDEBUG
               try {
#endif
                  while (!queue_->done()) {
                     queue_->tryProcess();
                  }
#ifdef NDEBUG
               }
               catch (const std::exception &e) {
                  const auto errMsg = std::string("Signer processing error: ") + e.what();
                  logger_->error("{}", errMsg);
                  std::cerr << errMsg << std::endl;
                  return EXIT_FAILURE;
               }
#endif   //NDEBUG

               QMetaObject::invokeMethod(qApp, [] {
                  QApplication::quit();
               });
            });
         }

         ~Queue()
         {
            appObj_.stop();
            if (thrProc_.joinable()) {
               thrProc_.join();
            }
            logger_->info("signer ended execution");
         }

      private:
         std::shared_ptr<spdlog::logger>  logger_;
         std::shared_ptr<DispatchQueue>   queue_;
         HeadlessAppObj appObj_;
         std::thread    thrProc_;
      };
   }  // namespace signer
}  // namespace bs


bool skipWarning(const QMessageLogContext &context)
{
   if (!context.function) {
      return false;
   }
   // Skip some warnings before this is fixed: https://bugreports.qt.io/browse/QTBUG-74523
   auto skipFunction = "QV4::ReturnedValue CallMethod(const QQmlObjectOrGadget&, int, int, int, int*, QV4::ExecutionEngine*, QV4::CallData*, QMetaObject::Call)";
   return std::strcmp(context.function, skipFunction) == 0;
}

static std::shared_ptr<spdlog::logger> logger;

// redirect qDebug() to the log
// stdout redirected to parent process
void qMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
   QByteArray localMsg = msg.toLocal8Bit();
   switch (type) {
   case QtDebugMsg:
      logger->debug("[QML] {}", localMsg.constData());
      break;
   case QtInfoMsg:
      logger->info("[QML] {}", localMsg.constData());
      break;
   case QtWarningMsg:
      logger->warn("[QML] {}", localMsg.constData());
      break;
   case QtCriticalMsg:
      logger->error("[QML] {}", localMsg.constData());
      break;
   case QtFatalMsg:
      logger->critical("[QML] {}", localMsg.constData());
      break;
   }
}

static int QMLApp(int argc, char **argv
   , const std::shared_ptr<HeadlessSettings> &mainSettings)
{
   qRegisterMetaType<std::string>();
   qRegisterMetaType<std::vector<BinaryData>>();
   qRegisterMetaType<BinaryData>();
   qRegisterMetaType<SecureBinaryData>();

   QGuiApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
   QApplication app(argc, argv);

   QApplication::setOrganizationDomain(QLatin1String("blocksettle.com"));
#ifdef __linux__
   // Needed for consistency (headless now uses company name in lowercase on Linux)
   QApplication::setOrganizationName(QLatin1String("blocksettle"));
   QApplication::setApplicationName(QLatin1String("signer"));
#else
   QApplication::setOrganizationName(QLatin1String("BlockSettle"));
   QApplication::setApplicationName(QLatin1String("Signer"));
#endif
   QApplication::setWindowIcon(QIcon(QStringLiteral(":/images/bs_logo.png")));

   // ToDo: support 2.0 styles
   // app.setStyle(QStyleFactory::create(QStringLiteral("Universal")));

   const auto settings = std::make_shared<SignerSettings>();
   if (!settings->loadSettings(mainSettings)) {
      return EXIT_FAILURE;
   }

   QSplashScreen *splashScreen = nullptr;

   // don't show slash screen on debug
#ifdef NDEBUG
   if (settings->runMode() == bs::signer::ui::RunMode::fullgui) {
      // we need this only for desktop app
      const auto splashImage = QPixmap(QLatin1String(":/FULL_LOGO")).scaledToWidth(390, Qt::SmoothTransformation);
      splashScreen = new QSplashScreen(splashImage);
      splashScreen->setWindowFlag(Qt::WindowStaysOnTopHint);

      splashScreen->show();
   }
#endif

   spdlog::drop("");
   try {
      logger = spdlog::basic_logger_mt("", settings->logFileName().toStdString());
      // [date time.miliseconds] [level](thread id): text
      logger->set_pattern(bs::LogManager::detectFormatOverride("%D %H:%M:%S.%e [%L](%t): %v"));
   } catch (const spdlog::spdlog_ex &e) {
      std::cerr << "Failed to create logger in "
         << settings->logFileName().toStdString() << ": " << e.what()
         << " - logging to console" << std::endl;
      logger = spdlog::stdout_logger_mt("");
      logger->set_pattern(bs::LogManager::detectFormatOverride("[%L](%t): %v"));
   }

#ifdef NDEBUG
   logger->set_level(spdlog::level::err);
   logger->flush_on(spdlog::level::err);
#else
   logger->set_level(spdlog::level::debug);
   logger->flush_on(spdlog::level::debug);
#endif

#ifndef NDEBUG
   qInstallMessageHandler(qMessageHandler);

#ifdef Q_OS_WIN
   // set zero buffer for stdout and stderr
   setvbuf(stdout, NULL, _IONBF, 0);
   setvbuf(stderr, NULL, _IONBF, 0);
#endif
#endif

   auto qmlBridge = std::make_shared<QmlBridge>(logger);

   // Go ahead and build the headless connection encryption files, even if we
   // don't use them. If they already exist, we'll leave them alone.
   logger->info("Starting BS Signer UI with args: {}", app.arguments().join(QLatin1Char(' ')).toStdString());
   QQmlApplicationEngine engine;
   QQuickWindow::setTextRenderType(QQuickWindow::NativeTextRendering);
   QWindowsWindowFunctions::setWindowActivationBehavior(QWindowsWindowFunctions::AlwaysActivateWindow);
   const QFont fixedFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
   engine.rootContext()->setContextProperty(QStringLiteral("fixedFont"), fixedFont);

   try {
      BinaryData srvIDKey(BIP151PUBKEYSIZE);
      if (!(settings->getSrvIDKeyBin(srvIDKey))) {
         logger->error("[{}] Unable to obtain server identity key from the "
            "command line. Functionality may be limited.", __func__);
      }

      SignerAdapter adapter(logger, qmlBridge, settings->netType(), settings->signerPort(), &srvIDKey);
      adapter.setCloseHeadless(settings->closeHeadless());

      QMLAppObj qmlAppObj(&adapter, logger, settings, splashScreen, engine.rootContext());
      QTimer::singleShot(0, &qmlAppObj, &QMLAppObj::Start);

      switch (settings->runMode()) {
      case bs::signer::ui::RunMode::fullgui:
         engine.load(QUrl(QStringLiteral("qrc:/qml/main.qml")));
         break;
      case bs::signer::ui::RunMode::litegui:
         engine.load(QUrl(QStringLiteral("qrc:/qml/mainLite.qml")));
         break;
      default:
         return EXIT_FAILURE;
      }

      if (engine.rootObjects().isEmpty()) {
         throw std::runtime_error("Failed to load main QML file");
      }
      qmlAppObj.SetRootObject(engine.rootObjects().at(0));
      qmlBridge->setRootQmlObj(engine.rootObjects().at(0));

      bs::disableAppNap();

      return app.exec();
   } catch (const std::exception &e) {
      logger->critical("Failed to start signer: {}", e.what());
      std::cerr << "Failed to start signer:" << e.what() << std::endl;
#ifdef NDEBUG
      return -1;
#else
      // Launch simple gui in debug mode (for development purposes)
      QMLAppObj qmlAppObj(nullptr, logger, settings, splashScreen, engine.rootContext());
      engine.load(QUrl(QStringLiteral("qrc:/qml/main.qml")));
      return app.exec();
#endif
   }
}


int main(int argc, char** argv)
{
   SystemFilePaths::setArgV0(argv[0]);

   srand(std::time(nullptr));

   btc_ecc_start();

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
   bs::signer::Queue queue(logger, settings);

   return QMLApp(argc, argv, settings);
}
