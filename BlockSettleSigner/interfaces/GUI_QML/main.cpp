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
#include <memory>
#include <iostream>
#include <btc/ecc.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_sinks.h>
#include "SignerAdapter.h"
#include "SignerSettings.h"
#include "QMLApp.h"
#include "ZMQ_BIP15X_ServerConnection.h"

Q_DECLARE_METATYPE(std::string)
Q_DECLARE_METATYPE(std::vector<BinaryData>)
Q_DECLARE_METATYPE(BinaryData)

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

// redirect qDebug() to stdout
// stdout redirected to parent process
void qMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    QByteArray localMsg = msg.toLocal8Bit();
    switch (type) {
    case QtDebugMsg:
        fprintf(stdout, "GUI QML Debug: %s\r\n", localMsg.constData());
        break;
    case QtInfoMsg:
        fprintf(stdout, "GUI QML Info: %s\r\n", localMsg.constData());
        break;
    case QtWarningMsg:
        fprintf(stderr, "GUI QML Warning: %s\r\n", localMsg.constData());
        break;
    case QtCriticalMsg:
        fprintf(stderr, "GUI QML Critical: %s\r\n", localMsg.constData());
        break;
    case QtFatalMsg:
        fprintf(stderr, "GUI QML Fatal: %s\r\n", localMsg.constData());
       break;
    }
}

static int QMLApp(int argc, char **argv)
{
   QGuiApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
   QApplication app(argc, argv);
   app.setApplicationName(QLatin1String("Signer"));
   app.setOrganizationDomain(QLatin1String("blocksettle.com"));
   app.setOrganizationName(QLatin1String("BlockSettle"));
   app.setWindowIcon(QIcon(QStringLiteral(":/images/bs_logo.png")));

   // ToDo: support 2.0 styles
   // app.setStyle(QStyleFactory::create(QStringLiteral("Universal")));

   // we need this only for desktop app
   const auto splashImage = QPixmap(QLatin1String(":/FULL_LOGO")).scaledToWidth(390, Qt::SmoothTransformation);
   QSplashScreen splashScreen(splashImage);
   splashScreen.setWindowFlag(Qt::WindowStaysOnTopHint);
#ifdef NDEBUG
   // don't show slash screen on debug
   splashScreen.show();
#endif

   const auto settings = std::make_shared<SignerSettings>();
   if (!settings->loadSettings(app.arguments())) {
      return EXIT_FAILURE;
   }

#ifndef NDEBUG
   qInstallMessageHandler(qMessageHandler);

#ifdef Q_OS_WIN
   // set zero buffer for stdout and stderr
   setvbuf(stdout, NULL, _IONBF, 0 );
   setvbuf(stderr, NULL, _IONBF, 0 );
#endif
#endif

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
   logger->info("Starting BS Signer UI with args: {}", app.arguments().join(QLatin1Char(' ')).toStdString());
   try {
      SignerAdapter adapter(logger, settings->netType());
      QQmlApplicationEngine engine;
      const QFont fixedFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
      engine.rootContext()->setContextProperty(QStringLiteral("fixedFont"), fixedFont);

      QMLAppObj qmlAppObj(&adapter, logger, settings, &splashScreen, engine.rootContext());
      QTimer::singleShot(0, &qmlAppObj, &QMLAppObj::Start);

      switch (settings->runMode()) {
      case bs::signer::ui::RunMode::fullgui:
         engine.load(QUrl(QStringLiteral("qrc:/qml/main.qml")));
         break;
      case bs::signer::ui::RunMode::lightgui:
         engine.load(QUrl(QStringLiteral("qrc:/qml/mainLight.qml")));
         break;
      default:
         return EXIT_FAILURE;
      }

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

int main(int argc, char** argv)
{
   qRegisterMetaType<std::string>();
   qRegisterMetaType<std::vector<BinaryData>>();
   qRegisterMetaType<BinaryData>();

   btc_ecc_start(); // Initialize libbtc
   startupBIP151CTX();
   startupBIP150CTX(4, true);

   return QMLApp(argc, argv);
}
