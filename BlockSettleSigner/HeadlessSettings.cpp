#include <QCommandLineParser>
#include <QDir>
#include <QFile>
#include <QSettings>
#include <QStandardPaths>
#include <QMetaEnum>
#include <spdlog/spdlog.h>
#include "BtcDefinitions.h"
#include "BlockDataManagerConfig.h"
#include "BtcUtils.h"
#include "HeadlessSettings.h"
#include "SignerUiDefs.h"

namespace {
   static const QString listenName = QString::fromStdString("listen");
   static const QString listenHelp = QObject::tr("IP address to listen on");

   static const QString portName = QString::fromStdString("port");
   static const QString portHelp = QObject::tr("Specify command port number");

   static const QString logName = QString::fromStdString("log");
   static const QString logHelp = QObject::tr("Log file name (relative to temp dir)");

   static const QString walletsDirName = QString::fromStdString("dirwallets");
   static const QString walletsDirHelp = QObject::tr("Directory where wallets reside");

   static const QString testnetName = QString::fromStdString("testnet");
   static const QString testnetHelp = QObject::tr("Set bitcoin network type to testnet");

   static const QString mainnetName = QString::fromStdString("mainnet");
   static const QString mainnetHelp = QObject::tr("Set bitcoin network type to mainnet");

   static const QString signName = QString::fromStdString("sign");
   static const QString signHelp = QObject::tr("Sign transaction[s] from request file - auto toggles offline mode (headless mode only)");

   static const QString runModeName = QString::fromStdString("guimode");
   static QString runModeHelp = QObject::tr("GUI run mode [__modes__]");

   static const QString autoSignLimitName = QString::fromStdString("auto_sign_spend_limit");
   static const QString autoSignLimitHelp = QObject::tr("Spend limit expressed in XBT for auto-sign operations");

   static const QString woName = QString::fromStdString("watchonly");
   static const QString woHelp = QObject::tr("Try to load only watching-only wallets");
}


HeadlessSettings::HeadlessSettings(const std::shared_ptr<spdlog::logger> &logger)
   : logger_(logger)
{
   QDir logDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
   logDir.cdUp();
   const auto writableDir = logDir.path().toStdString();

   logFile_ = writableDir + "/bs_signer.log";
}

bool HeadlessSettings::loadSettings(const QStringList &args)
{
   // substitute run modes from RunMode enum to help output
   QMetaEnum runModesEnum = QMetaEnum::fromType<bs::signer::ui::RunMode>();
   QStringList runModes;
   for (int i = 0; i < runModesEnum.keyCount(); ++i) {
      runModes.append(QString::fromLatin1(runModesEnum.valueToKey(i)));
   }
   runModeHelp.replace(QStringLiteral("__modes__"), runModes.join(QStringLiteral("|")));

   {
      QDir logDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
      logDir.cdUp();
      const auto writableDir = logDir.path();
      QSettings ini(writableDir + QLatin1String("/signer.ini"), QSettings::IniFormat);

      watchOnly_ = ini.value(QStringLiteral("WatchingOnly"), watchOnly_).toBool();
      testNet_ = ini.value(QStringLiteral("TestNet"), testNet_).toBool();
      walletsDir_ = ini.value(QStringLiteral("WalletsDir"), QString::fromStdString(walletsDir_)).toString().toStdString();
      logFile_ = ini.value(QStringLiteral("LogFileName"), QString::fromStdString(logFile_)).toString().toStdString();
      listenAddress_ = ini.value(QStringLiteral("ListenAddress"), QString::fromStdString(listenAddress_)).toString().toStdString();
      listenPort_ = ini.value(QStringLiteral("ListenPort"), QString::fromStdString(listenPort_)).toString().toStdString();
      trustedTerminals_ = ini.value(QStringLiteral("TrustedTerminals")).toStringList();
   }

   QCommandLineParser parser;
   parser.setApplicationDescription(QObject::tr("BlockSettle Signer"));
   parser.addHelpOption();
   parser.addOption({ listenName, listenHelp, QObject::tr("ip/host") });
   parser.addOption({ portName, portHelp, QObject::tr("port") });
   parser.addOption({ logName, logHelp, QObject::tr("log") });
   parser.addOption({ walletsDirName, walletsDirHelp, QObject::tr("dir") });
   parser.addOption({ testnetName, testnetHelp });
   parser.addOption({ mainnetName, mainnetHelp });
   parser.addOption({ autoSignLimitName, autoSignLimitHelp, QObject::tr("limit") });
   parser.addOption({ runModeName, runModeHelp, runModeName });
   parser.addOption({ woName, woHelp });

   parser.process(args);

   if (parser.isSet(listenName)) {
      listenAddress_ = parser.value(listenName).toStdString();
   }

   if (parser.isSet(portName)) {
      listenPort_ = parser.value(portName).toStdString();
   }

   if (parser.isSet(logName)) {
      logFile_ = parser.value(logName).toStdString();
   }

   if (parser.isSet(walletsDirName)) {
      walletsDir_ = parser.value(walletsDirName).toStdString();
   }

   if (parser.isSet(runModeName)) {
      int runModeValue = runModesEnum.keyToValue(parser.value(runModeName).toLatin1());
      if (runModeValue < 0) {
         return false;
      }
      runMode_ = static_cast<bs::signer::ui::RunMode>(runModeValue);
   } else {
      runMode_ = bs::signer::ui::RunMode::fullgui;
   }

   if (parser.isSet(mainnetName)) {
      testNet_ = false;
   } else if (parser.isSet(testnetName)) {
      testNet_ = true;
   }

   if (parser.isSet(woName)) {
      watchOnly_ = true;
   }

   if (parser.isSet(autoSignLimitName)) {
      const auto val = parser.value(autoSignLimitName).toDouble();
      if (val > 0) {
         autoSignSpendLimit_ = val;
      }
   }

   NetworkConfig config;
   if (testNet()) {
      config.selectNetwork(NETWORK_MODE_TESTNET);
   } else {
      config.selectNetwork(NETWORK_MODE_MAINNET);
   }
   return true;
}

NetworkType HeadlessSettings::netType() const
{
   if (testNet()) {
      return NetworkType::TestNet;
   }
   return NetworkType::MainNet;
}

static const QString testnetSubdir = QLatin1String("testnet3");
#if defined (Q_OS_WIN)
static const QString appDirName = QLatin1String("Blocksettle");
#elif defined (Q_OS_OSX)
static const QString appDirName = QLatin1String("Blocksettle");
#elif defined (Q_OS_LINUX)
static const QString appDirName = QLatin1String(".blocksettle");
#endif

std::string HeadlessSettings::getWalletsDir() const
{
   if (!walletsDir_.empty()) {
      return walletsDir_;
   }
   const auto dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
   const auto commonRoot = dir + QDir::separator() + QLatin1String("..") + QDir::separator()
      + QLatin1String("..") + QDir::separator() + appDirName;

   QString result;
   if (testNet()) {
      result = commonRoot + QDir::separator() + testnetSubdir;
   } else {
      result = commonRoot;
   }
   result += QDir::separator() + QLatin1String("signer");
   return QDir::cleanPath(result).toStdString();
}

SignContainer::Limits HeadlessSettings::limits() const
{
   SignContainer::Limits result;
   result.autoSignSpendXBT = autoSignSpendLimit_ * BTCNumericTypes::BalanceDivider;
   return result;
}

QStringList HeadlessSettings::trustedInterfaces() const
{
   QStringList result;
   const auto dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
   QFile pubKeyFile(dir + QLatin1String("/interface.pub"));
   if (pubKeyFile.exists()) {
      if (pubKeyFile.open(QIODevice::ReadOnly)) {
         const auto data = pubKeyFile.readAll();
         result << QString::fromStdString("local:" + data.toStdString());
      }
   }
   return result;
}
