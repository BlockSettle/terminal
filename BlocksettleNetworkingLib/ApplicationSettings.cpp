/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "ApplicationSettings.h"

#include "BlockDataManagerConfig.h"
#include "EncryptionUtils.h"
#include "FastLock.h"
#include "ArmoryConnection.h"
#include "ArmoryServersProvider.h"

#include <QCommandLineParser>
#include <QRect>
#include <QDir>
#include <QStandardPaths>
#include <QtGlobal>

#if defined (Q_OS_WIN)
static const QString appDirName = QLatin1String("Blocksettle");
static const QString bitcoinDirName = QLatin1String("Bitcoin");
static const QString armoryDBAppPathName = QLatin1String("C:/Program Files/ArmoryDB/ArmoryDB.exe");
#elif defined (Q_OS_MACOS)
static const QString appDirName = QLatin1String("Blocksettle");
static const QString bitcoinDirName = QLatin1String("Bitcoin");
static const QString armoryDBAppPathName = QLatin1String("/usr/bin/ArmoryDB");
#elif defined (Q_OS_LINUX)
static const QString appDirName = QLatin1String("blocksettle");
static const QString bitcoinDirName = QLatin1String(".bitcoin");
static const QString armoryDBAppPathName = QLatin1String("/usr/bin/ArmoryDB");
#endif

#ifdef __linux__
// Needed for consistency (headless now uses company name in lowercase on Linux)
static const QString SettingsCompanyName = QLatin1String("blocksettle");
#else
static const QString SettingsCompanyName = QLatin1String("BlockSettle");
#endif

static const QString LogFileName = QLatin1String("bs_terminal.log");
static const QString LogMsgFileName = QLatin1String("bs_terminal_messages.log");
static const QString TxCacheFileName = QLatin1String("transactions.cache");

static const QString blockDirName = QLatin1String("blocks");
static const QString databasesDirName = QLatin1String("databases");

static const QString dataDirName = QLatin1String("datadir");
static const QString dataDirHelp = QLatin1String("Change the directory that Armory calls home");
static const QString satoshiDataDirName = QLatin1String("satoshi-datadir");
static const QString satoshiDataDirHelp = QLatin1String("The Bitcoin-Core/bitcoind home directory");
static const QString blockDBDirName = QLatin1String("dbdir");
static const QString blockDBDirHelp = QLatin1String("Location to store blocks database (defaults to --datadir");
static const QString testnetName = QLatin1String("testnet");
static const QString testnetHelp = QLatin1String("Use the testnet protocol");
static const QString nonSpendZeroConfName = QLatin1String("nospendzeroconfchange");
static const QString nonSpendZeroConfHelp = QLatin1String("All zero-conf funds will be unspendable, including sent-to-self coins");

static const QString armoryDBIPName = QLatin1String("armorydb-ip");
static const QString armoryDBIPHelp = QLatin1String("ArmoryDB host ip");
static const QString armoryDBPortName = QLatin1String("armorydb-port");
static const QString armoryDBPortHelp = QLatin1String("ArmoryDB port");

static const QString groupRescan = QLatin1String("rescan");

static const QString testnetSubdir = QLatin1String("testnet3");
static const QString regtestSubdir = QLatin1String("regtest");

static const QString zmqSignerKeyFileName = QLatin1String("zmq_conn_srv.pub");

static const int ArmoryDefaultLocalMainPort = 9001;
static const int ArmoryDefaultLocalTestPort = 19001;
static const int ArmoryDefaultRemoteMainPort = 9001;
static const int ArmoryDefaultRemoteTestPort = 19001;

#ifndef NDEBUG
static const QString chatServerIPName = QLatin1String("chatserver-ip");
static const QString chatServerIPHelp = QLatin1String("Chat servcer host ip");
static const QString chatServerPortName = QLatin1String("chatserver-port");
static const QString chatServerPortHelp = QLatin1String("Chat server port");

static const QString localSignerPortName = QLatin1String("local-signer-port");
static const QString localSignerPortHelp = QLatin1String("Local signer port");
#endif // NDEBUG

namespace {

#ifdef NDEBUG
   const char DefaultLogLevel[] = "error";
#else
   const char DefaultLogLevel[] = "debug";
#endif

} // namespace


ApplicationSettings::ApplicationSettings(const QString &appName
   , const QString& rootDir)
   : settings_(QSettings::IniFormat, QSettings::UserScope, SettingsCompanyName, appName)
{
   if (rootDir.isEmpty()) {
      commonRoot_ = AppendToWritableDir(QLatin1String(".."));
   } else {
      commonRoot_ = rootDir;
   }

   settingDefs_ = {
      { initialized,             SettingDef(QLatin1String("SettingsAccepted"), false) },
      { runArmoryLocally,        SettingDef(QLatin1String("RunArmoryLocally"), false) },
      { netType,                 SettingDef(QLatin1String("Testnet"), (int)NetworkType::MainNet) },
      { armoryDbName,            SettingDef(QLatin1String("ArmoryDBName"), QLatin1String(ARMORY_BLOCKSETTLE_NAME)) },
      { armoryDbIp,              SettingDef(QLatin1String("ArmoryDBIP"), QLatin1String(MAINNET_ARMORY_BLOCKSETTLE_ADDRESS)) },
      { armoryDbPort,            SettingDef(QLatin1String("ArmoryDBPort"), MAINNET_ARMORY_BLOCKSETTLE_PORT) },
      { armoryPathName,          SettingDef(QString(), armoryDBAppPathName) },
      { customPubBridgeHost,     SettingDef(QLatin1String("CustomPubBridgeHost"), QString()) },
      { customPubBridgePort,     SettingDef(QLatin1String("CustomPubBridgePort"), 9091) },
      { pubBridgePubKey,         SettingDef(QLatin1String("PubBridgePubKey"), QString()) },
   #ifdef PRODUCTION_BUILD
      { envConfiguration,        SettingDef(QLatin1String("envConfiguration"), int(PROD)) },
   #else
      { envConfiguration,        SettingDef(QLatin1String("envConfiguration"), int(Staging)) },
   #endif
      { mdServerHost,            SettingDef(QString()) },
      { mdServerPort,            SettingDef(QString()) },
      { mdhsHost,                SettingDef(QString()) },
      { mdhsPort,                SettingDef(QString()) },
      { chatServerHost,          SettingDef(QString()) },
      { chatServerPort,          SettingDef(QString()) },
      { chatServerPubKey,        SettingDef(QLatin1String("ChatServerPubKey"), QString()) },
      { chatDbFile,              SettingDef(QString(), AppendToWritableDir(QLatin1String("chat2.db"))) },
      { celerUsername,           SettingDef(QLatin1String("MatchSystemUsername")) },
      { localSignerPort,         SettingDef(QLatin1String("SignerPort"), 23456) },
      { signerIndex,             SettingDef(QLatin1String("SignerIndex"), 0) },
      { signerOfflineDir,        SettingDef(QLatin1String("SignerOfflineDir"), QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)) },
      { autoSignSpendLimit,      SettingDef(QLatin1String("AutoSignSpendLimit"), 0.0) },
      { launchToTray,            SettingDef(QLatin1String("LaunchToTray"), false) },
      { minimizeToTray,          SettingDef(QLatin1String("MinimizeToTray"), false) },
      { closeToTray,             SettingDef(QLatin1String("CloseToTray"), false) },
      { notifyOnTX,              SettingDef(QLatin1String("ShowTxNotification"), true) },
      { defaultAuthAddr,         SettingDef(QLatin1String("DefaultAuthAddress")) },
      { bsPublicKey,             SettingDef(QString(), QLatin1String("022aa8719eadf13ba5bbced2848fb492a4118087b200fdde8ec68a2f5d105b36fa")) },
      { logDefault,              SettingDef(QLatin1String("LogFile"), QStringList() << LogFileName << QString() << QString() << QLatin1String(DefaultLogLevel)) },
      { logMessages,             SettingDef(QLatin1String("LogMsgFile"), QStringList() << LogMsgFileName << QLatin1String("message") << QLatin1String("%C/%m/%d %H:%M:%S.%e [%L]: %v") << QLatin1String(DefaultLogLevel)) },
      { txCacheFileName,         SettingDef(QString(), AppendToWritableDir(TxCacheFileName)) },
      { nbBackupFilesKeep,       SettingDef(QString(), 10) },
      { aqScripts,               SettingDef(QLatin1String("AutoQuotingScripts")) },
      { lastAqScript,            SettingDef(QLatin1String("LastAutoQuotingScript")) },
      { dropQN,                  SettingDef(QLatin1String("DropQNifSecUnavail"), false) },
      { GUI_main_geometry,       SettingDef(QLatin1String("GUI/main/geometry")) },
      { GUI_main_tab,            SettingDef(QLatin1String("GUI/main/tab")) },
      { ChartProduct,            SettingDef(QLatin1String("GUI/chart/product"), QLatin1String("XBT/EUR")) },
      { ChartTimeframe,          SettingDef(QLatin1String("GUI/chart/timeframe"), 7) },
      { ChartCandleCount,        SettingDef(QLatin1String("GUI/chart/candleCount"), 150) },
      { Filter_MD_RFQ,           SettingDef(QLatin1String("Filter/MD/RFQ")) },
      { Filter_MD_RFQ_Portfolio, SettingDef(QLatin1String("Filter/MD/RFQ_Portfolio")) },
      { Filter_MD_QN,            SettingDef(QLatin1String("Filter/MD/QN")) },
      { Filter_MD_QN_cnt,        SettingDef(QLatin1String("Filter/MD/QN/counters")) },
      { ChangeLog_Base_Url,      SettingDef(QString(), QLatin1String("https://pubb.blocksettle.com/Changelog/changelog.json"))},
      { Binaries_Dl_Url,         SettingDef(QString(), QLatin1String("https://pubb.blocksettle.com/downloads/terminal"))},
      { ResetPassword_Url,       SettingDef(QString(), QLatin1String("https://pubb.blocksettle.com/pub-forgot-password"))},
      { GetAccount_Url,          SettingDef(QString(), QLatin1String("http://www.blocksettle.com")) },
      { GettingStartedGuide_Url, SettingDef(QString(), QLatin1String("http://pubb.blocksettle.com/PDF/BlockSettle%20Getting%20Started.pdf")) },
      { WalletFiltering,         SettingDef(QLatin1String("WalletWidgetFilteringFlags"), 0x06) },
      { FxRfqLimit,              SettingDef(QLatin1String("FxRfqLimit"), 5) },
      { XbtRfqLimit,             SettingDef(QLatin1String("XbtRfqLimit"), 5) },
      { PmRfqLimit,              SettingDef(QLatin1String("PmRfqLimit"), 5) },
      { PriceUpdateInterval,     SettingDef(QLatin1String("PriceUpdateInterval"), -1) },
      { ShowQuoted,              SettingDef(QLatin1String("ShowQuoted"), true) },
      { DisableBlueDotOnTabOfRfqBlotter,  SettingDef(QLatin1String("DisableBlueDotOnTabOfRfqBlotter"), false) },
      { AdvancedTxDialogByDefault,        SettingDef(QLatin1String("AdvancedTxDialogByDefault"), false) },
      { TransactionFilter,                SettingDef(QLatin1String("TransactionFilter"), QVariantList() << QStringList() << 0) },
      { SubscribeToMDOnStart,             SettingDef(QLatin1String("SubscribeToMDOnStart"), false) },
      { MDLicenseAccepted,                SettingDef(QLatin1String("MDLicenseAccepted"), false) },
      { authPrivKey,                      SettingDef(QLatin1String("AuthPrivKey")) },
      { zmqLocalSignerPubKeyFilePath,     SettingDef(QLatin1String("ZmqLocalSignerPubKeyFilePath"), AppendToWritableDir(zmqSignerKeyFileName)) },
      { remoteSigners,                    SettingDef(QLatin1String("RemoteSigners"), QStringList()
         << QString::fromLatin1("%1:127.0.0.1:23456:").arg(localSignerDefaultName())) },
      { rememberLoginUserName,            SettingDef(QLatin1String("RememberLoginUserName"), true) },
      { armoryServers,                    SettingDef(QLatin1String("ArmoryServers")) },
      { defaultArmoryServersKeys,         SettingDef(QLatin1String("DefaultArmoryServersKeys"), QStringList()
         << QLatin1String("02b4abf18a3ea48550e77a7e017394d3f31df276123922cf9c73568d31eca59e93")       // mainnet Armory cluster key
         << QLatin1String("02ed6116a7844cae8a1dc4d5fb27922594b79cc41df081d84d2f36983757904de5")) },   // testnet Armory cluster key
      { twoWaySignerAuth,        SettingDef(QLatin1String("TwoWaySignerAuth"), true) },
      { proxyServerPubKey,       SettingDef(QLatin1String("ProxyServerPubKey"), QString()) },
      { LastAqDir,               SettingDef(QLatin1String("LastAqDir")) },
   };
}

QVariant ApplicationSettings::get(Setting set, bool getDefaultValue) const
{
   FastLock lock(lock_);
   auto itSD = settingDefs_.find(set);
   if (itSD == settingDefs_.end()) {
      return QVariant();
   }

   // we just need default value. no need to change current settings
   // need for ConfigDialog to display default settings and do not change current.
   // so user could press cancel
   if (getDefaultValue) {
      return itSD->second.defVal;
   }

   // lazy initialization
   if (itSD->second.read) {
      return itSD->second.value;
   }

   // if this setting is not set by command line parameters and do not have name ( could not be
   // loaded from file) we return default value. It will be lost after application restart
   if (itSD->second.path.isEmpty()) {
      itSD->second.value = itSD->second.defVal;
   } else {
      itSD->second.value = settings_.value(itSD->second.path, itSD->second.defVal);
   }

   itSD->second.read = true;
   return itSD->second.value;
}

bool ApplicationSettings::isDefault(Setting set) const
{
   FastLock lock(lock_);
   auto itSD = settingDefs_.find(set);
   if (itSD == settingDefs_.end()) {
      return false;
   }

   if (!itSD->second.read) {
      return true;
   }

   return (itSD->second.value == itSD->second.defVal);
}

void ApplicationSettings::set(Setting s, const QVariant &val, bool toFile)
{
   bool changed = false;

   if (val.isValid()) {
      FastLock lock(lock_);
      auto itSD = settingDefs_.find(s);

      if (itSD != settingDefs_.end()) {
         itSD->second.read = true;
         if (val != itSD->second.value) {
            itSD->second.value = val;
            changed = true;
         }

         if (toFile && !itSD->second.path.isEmpty()) {
            settings_.setValue(itSD->second.path, val);
         }
      }
   }

   lock_.clear();
   if (changed) {
      emit settingChanged(s, val);
   }
}

void ApplicationSettings::reset(Setting s, bool toFile)
{
   FastLock lock(lock_);
   auto itSD = settingDefs_.find(s);

   if (itSD != settingDefs_.end()) {
      itSD->second.read = true;
      if (itSD->second.value != itSD->second.defVal) {
         itSD->second.value = itSD->second.defVal;
         emit settingChanged(s, itSD->second.defVal);
      }

      if (toFile && !itSD->second.path.isEmpty()) {
         settings_.setValue(itSD->second.path, itSD->second.value);
      }
   }
}

template<typename T> T ApplicationSettings::get(Setting set, bool getDefaultValue) const
{
   return get(set, getDefaultValue);
}

template<> QString ApplicationSettings::get<QString>(Setting set, bool getDefaultValue) const
{
   return get(set, getDefaultValue).toString();
}

template<> std::string ApplicationSettings::get<std::string>(Setting set, bool getDefaultValue) const
{
   return get(set, getDefaultValue).toString().toStdString();
}

template<> bool ApplicationSettings::get<bool>(Setting set, bool getDefaultValue) const
{
   return get(set, getDefaultValue).toBool();
}

template<> int ApplicationSettings::get<int>(Setting set, bool getDefaultValue) const
{
   return get(set, getDefaultValue).toInt();
}

template<> unsigned int ApplicationSettings::get<unsigned int>(Setting set, bool getDefaultValue) const
{
   return get(set, getDefaultValue).toUInt();
}

template<> double ApplicationSettings::get<double>(Setting set, bool getDefaultValue) const
{
   return get(set, getDefaultValue).toDouble();
}

template<> QRect ApplicationSettings::get<QRect>(Setting set, bool getDefaultValue) const
{
   return get(set, getDefaultValue).toRect();
}
template<> QStringList ApplicationSettings::get<QStringList>(Setting set, bool getDefaultValue) const
{
   return get(set, getDefaultValue).toStringList();
}

template<> QVariantMap ApplicationSettings::get<QVariantMap> (Setting set, bool getDefaultValue) const
{
   return get(set, getDefaultValue).toMap();
}

template<> NetworkType ApplicationSettings::get<NetworkType>(Setting set, bool getDefaultValue) const
{
   const int result = get(set, getDefaultValue).toInt();
   if ((result < (int)NetworkType::first) || (result >= (int)NetworkType::last)) {
      return NetworkType::Invalid;
   }
   return static_cast<NetworkType>(result);
}

ApplicationSettings::State ApplicationSettings::getState() const
{
   State result;
   for (const auto &settingDef : settingDefs_) {
      result[settingDef.first] = get(settingDef.first);
   }
   return result;
}

void ApplicationSettings::setState(const State &state)
{
   for (const auto &setting : state) {
      set(setting.first, setting.second, false);
   }
}

QString ApplicationSettings::GetSettingsPath() const
{
   return settings_.fileName();
}

bool ApplicationSettings::LoadApplicationSettings(const QStringList& argList)
{
   QCommandLineParser parser;

   parser.addOption({ testnetName , testnetHelp });
   parser.addOption({ dataDirName , dataDirHelp, QLatin1String("ddir") });
   parser.addOption({ satoshiDataDirName , satoshiDataDirHelp, QLatin1String("btcdir") });
   parser.addOption({ blockDBDirName , blockDBDirHelp, QLatin1String("dbdir") });
   parser.addOption({ armoryDBIPName, armoryDBIPHelp, QLatin1String("dbip") });
   parser.addOption({ armoryDBPortName, armoryDBPortHelp, QLatin1String("dbport") });
   parser.addOption({ nonSpendZeroConfName, nonSpendZeroConfHelp });

#ifndef NDEBUG
   parser.addOption({ chatServerIPName, chatServerIPHelp,  QLatin1String("chatip") });
   parser.addOption({ chatServerPortName, chatServerPortHelp, QLatin1String("chatport") });
   parser.addOption({ localSignerPortName, localSignerPortHelp, QLatin1String("localsignerport") });
#endif // NDEBUG



   if (!parser.parse(argList)) {
      errorText_ = parser.errorText();
      return false;
   }

   if (parser.isSet(testnetName)) {
      set(netType, (int)NetworkType::TestNet);
   }

   selectNetwork();

   SetHomeDir(parser.value(dataDirName));
   SetBitcoinsDir(parser.value(satoshiDataDirName));
   SetDBDir(parser.value(blockDBDirName));

   if (parser.isSet(armoryDBIPName)) {
      set(armoryDbIp, parser.value(armoryDBIPName));
   }
   if (parser.isSet(armoryDBPortName)) {
      set(armoryDbPort, parser.value(armoryDBPortName).toInt());
   }

#ifndef NDEBUG
   if (parser.isSet(chatServerIPName)) {
      QString vcip = parser.value(chatServerIPName);
      set(chatServerHost, vcip);
   }
   if (parser.isSet(chatServerPortName)) {
      int vcp = parser.value(chatServerPortName).toInt();
      set(chatServerPort, vcp);
   }
   if (parser.isSet(localSignerPortName)) {
      int value = parser.value(localSignerPortName).toInt();
      set(localSignerPort, value);
   }
#endif // NDEBUG


   settings_.sync();

   return true;
}

void ApplicationSettings::SetDefaultSettings(bool toFile)
{
   reset(envConfiguration, toFile);
   reset(customPubBridgeHost, toFile);
   reset(customPubBridgePort, toFile);

   reset(launchToTray, toFile);
   reset(minimizeToTray, toFile);
   reset(closeToTray, toFile);
   reset(notifyOnTX, toFile);

   reset(logDefault);
   reset(logMessages);

   if (toFile) {
      set(initialized, true);
      SaveSettings();
   }
}

QString ApplicationSettings::GetDefaultHomeDir() const
{
   switch (get<NetworkType>(netType)) {
   case NetworkType::TestNet:
      return QDir::cleanPath(commonRoot_ + QDir::separator() + appDirName + QDir::separator() + testnetSubdir);
   case NetworkType::RegTest:
      return QDir::cleanPath(commonRoot_ + QDir::separator() + appDirName + QDir::separator() + regtestSubdir);
   default:
      return QDir::cleanPath(commonRoot_ + QDir::separator() + appDirName);
   }
}

QString ApplicationSettings::GetHomeDir() const
{
   const QString dir = dataDir_.isEmpty() ? GetDefaultHomeDir() : dataDir_;
   QDir().mkpath(dir);
   return dir;
}

void ApplicationSettings::SetHomeDir(const QString& path)
{
   if (!path.isEmpty()) {
      dataDir_ = QDir::cleanPath(path);
   }
}

QString ApplicationSettings::GetBackupDir() const
{
   return QDir::cleanPath(commonRoot_ + QDir::separator() + appDirName + QDir::separator() + QLatin1String("backup"));
}

QString ApplicationSettings::GetDefaultBitcoinsDir() const
{
#ifdef Q_OS_LINUX
   QString bitcoinRoot = QDir::homePath();
#else
   QString bitcoinRoot = commonRoot_;
#endif
   switch (get<NetworkType>(netType)) {
   case NetworkType::TestNet:
      return QDir::cleanPath(bitcoinRoot + QDir::separator() + bitcoinDirName + QDir::separator() + testnetSubdir);
   case NetworkType::RegTest:
      return QDir::cleanPath(bitcoinRoot + QDir::separator() + bitcoinDirName + QDir::separator() + regtestSubdir);
   default:
      return QDir::cleanPath(bitcoinRoot + QDir::separator() + bitcoinDirName);
   }
}

QString ApplicationSettings::GetBitcoinBlocksDir() const
{
   const QString dir = bitcoinsDir_.isEmpty() ? GetDefaultBitcoinsDir() : bitcoinsDir_;
   QDir().mkpath(dir);
   return QDir::cleanPath(dir + QDir::separator() + blockDirName);
}

void ApplicationSettings::SetBitcoinsDir(const QString& path)
{
   if (!path.isEmpty()) {
      bitcoinsDir_ = QDir::cleanPath(path);
   }
}

QString ApplicationSettings::GetDefaultDBDir() const
{
   return QDir::cleanPath(GetDefaultHomeDir() + QDir::separator() + databasesDirName);
}

QString ApplicationSettings::GetDBDir() const
{
   const QString dir = dbDir_.isEmpty() ? GetDefaultDBDir() : dbDir_;
   QDir().mkpath(dir);
   return dir;
}

void ApplicationSettings::SetDBDir(const QString &path)
{
   if (!path.isEmpty()) {
      dbDir_ = QDir::cleanPath(path);
   }
}

SocketType ApplicationSettings::GetArmorySocketType() const
{
   return SocketWS;
}

int ApplicationSettings::GetDefaultArmoryRemotePort(NetworkType networkType)
{
   switch (networkType) {
   case NetworkType::MainNet:
      return ArmoryDefaultRemoteMainPort;
   case NetworkType::TestNet:
      return ArmoryDefaultRemoteTestPort;
   default:
      return 0;
   }
}

int ApplicationSettings::GetArmoryRemotePort(NetworkType networkType) const
{
   int port;
   port = get<int>(ApplicationSettings::armoryDbPort);
   if (port == 0) {
      port = GetDefaultArmoryRemotePort(
         (networkType == NetworkType::Invalid) ? get<NetworkType>(netType) : networkType);
   }
   return port;
}

QString ApplicationSettings::localSignerDefaultName()
{
   return tr("Local GUI mode");
}

int ApplicationSettings::GetDefaultArmoryLocalPort(NetworkType networkType)
{
   switch (networkType) {
   case NetworkType::MainNet:
      return ArmoryDefaultLocalMainPort;
   case NetworkType::TestNet:
      return ArmoryDefaultLocalTestPort;
   default:
      return 0;
   }
}

QString ApplicationSettings::AppendToWritableDir(const QString &filename) const
{
   const auto dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
   QDir().mkpath(dir);
   return QDir::cleanPath(dir + QDir::separator() + filename);
}

void ApplicationSettings::SaveSettings()
{
   settings_.sync();
}

unsigned int ApplicationSettings::GetWalletScanIndex(const std::string &id) const
{
   FastLock lock(lock_);
   return settings_.value(groupRescan + QString::fromStdString("/" + id), UINT32_MAX).toUInt();
}

void ApplicationSettings::SetWalletScanIndex(const std::string &id, unsigned int index)
{
   FastLock lock(lock_);
   if (id.empty()) {
      settings_.remove(groupRescan);
   }
   else {
      if (index == UINT32_MAX) {
         settings_.remove(groupRescan + QString::fromStdString("/" + id));
      }
      else {
         settings_.setValue(groupRescan + QString::fromStdString("/" + id), index);
      }
   }
}

std::vector<std::pair<std::string, unsigned int>>  ApplicationSettings::UnfinishedWalletsRescan()
{
   std::vector<std::pair<std::string, unsigned int>> result;
   FastLock lock(lock_);
   settings_.beginGroup(groupRescan);
   for (const auto &key : settings_.allKeys()) {
      result.push_back({ key.toStdString(), settings_.value(key).toUInt() });
   }
   settings_.endGroup();
   return result;
}

std::vector<bs::LogConfig> ApplicationSettings::GetLogsConfig() const
{
   std::vector<bs::LogConfig> result;
   result.push_back(parseLogConfig(get<QStringList>(ApplicationSettings::logDefault)));
   result.push_back(parseLogConfig(get<QStringList>(ApplicationSettings::logMessages)));
   return result;
}

bs::LogConfig ApplicationSettings::parseLogConfig(const QStringList &config) const
{
   bs::LogConfig result;
   if (config.size() > 0) {
      if (QDir::toNativeSeparators(config[0]).contains(QDir::separator())) {
         result.fileName = QDir().absoluteFilePath(config[0]).toStdString();
      } else {
         result.fileName = AppendToWritableDir(config[0]).toStdString();
      }
   }
   if (config.size() > 1) {
      result.category = config[1].toStdString();
   }
   if (config.size() > 2) {
      result.pattern = config[2].toStdString();
   }
   if (config.size() > 3) {
      result.level = parseLogLevel(config[3]);
   }
   return result;
}

bs::LogLevel ApplicationSettings::parseLogLevel(QString level) const
{
   level = level.toLower();
   if (level.contains(QLatin1String("trace"))) {
      return bs::LogLevel::trace;
   }
   if (level.contains(QLatin1String("debug"))) {
      return bs::LogLevel::debug;
   }
   if (level.contains(QLatin1String("info"))) {
      return bs::LogLevel::info;
   }
   if (level.contains(QLatin1String("warn"))) {
      return bs::LogLevel::warn;
   }
   if (level.contains(QLatin1String("error"))) {
      return bs::LogLevel::err;
   }
   if (level.contains(QLatin1String("crit"))) {
      return bs::LogLevel::crit;
   }
   return bs::LogLevel::debug;
}

QString ApplicationSettings::ccFilePath() const
{
   auto conf = EnvConfiguration(get<int>(ApplicationSettings::envConfiguration));
   auto netType = get<NetworkType>(ApplicationSettings::netType);
   auto fileName = fmt::format("ccgenaddr-{}-{}.signed", envName(conf), networkName(netType));
   return AppendToWritableDir(QString::fromStdString(fileName));
}

std::pair<autheid::PrivateKey, autheid::PublicKey> ApplicationSettings::GetAuthKeys()
{
   if (!authPrivKey_.empty() && !authPubKey_.empty()) {
      return { authPrivKey_, authPubKey_ };
   }

   SecureBinaryData privKey = BinaryData::CreateFromHex(get<std::string>(authPrivKey));
   if (privKey.getSize() == autheid::kPrivateKeySize) {
      const auto sPrivKey = privKey.toBinStr();
      authPrivKey_ = autheid::SecureBytes(sPrivKey.begin(), sPrivKey.end());
   }
   else {
      authPrivKey_ = autheid::generatePrivateKey();
      const std::string sPrivKey(authPrivKey_.begin(), authPrivKey_.end());
      set(authPrivKey, QString::fromStdString(BinaryData(sPrivKey).toHexStr()));
   }
   authPubKey_ = autheid::getPublicKey(authPrivKey_);
   return { authPrivKey_, authPubKey_ };
}

std::string ApplicationSettings::pubBridgeHost() const
{
   auto env = EnvConfiguration(get<int>(ApplicationSettings::envConfiguration));

   switch (env) {
      case PROD:
         return "185.213.153.36";
      case UAT:
         return "185.213.153.44";
      case Staging:
         return "185.213.153.45";
      case Custom:
         return get<std::string>(ApplicationSettings::customPubBridgeHost);
   }

   assert(false);
   return "";
}

std::string ApplicationSettings::pubBridgePort() const
{
   auto env = EnvConfiguration(get<int>(ApplicationSettings::envConfiguration));

   switch (env) {
      case PROD:
      case UAT:
      case Staging:
         return "9091";
      case Custom:
         return get<std::string>(ApplicationSettings::customPubBridgePort);
   }

   assert(false);
   return "";
}

void ApplicationSettings::selectNetwork()
{
   // Set up Armory as needed. Even though the BDMC object isn't used, it sets
   // global values that are used later.
   BlockDataManagerConfig config;

   switch (get<NetworkType>(netType)) {
   case NetworkType::MainNet:
      config.selectNetwork(NETWORK_MODE_MAINNET);
      break;

   case NetworkType::TestNet:
      config.selectNetwork(NETWORK_MODE_TESTNET);
      break;

   case NetworkType::RegTest:
      config.selectNetwork(NETWORK_MODE_REGTEST);
      break;

   default:
      assert(false);
      break;
   }
}

bool ApplicationSettings::isAutheidTestEnv() const
{
   auto conf = ApplicationSettings::EnvConfiguration(get<int>(ApplicationSettings::envConfiguration));

   switch (conf) {
      case ApplicationSettings::EnvConfiguration::UAT:
      case ApplicationSettings::EnvConfiguration::Staging:
      case ApplicationSettings::EnvConfiguration::Custom:
         return true;
      default:
         return false;
   }
}

// static
std::string ApplicationSettings::envName(ApplicationSettings::EnvConfiguration conf)
{
   switch (conf) {
      case ApplicationSettings::EnvConfiguration::PROD:
         return "prod";
      case ApplicationSettings::EnvConfiguration::UAT:
         return "uat";
      case ApplicationSettings::EnvConfiguration::Staging:
         return "staging";
      case ApplicationSettings::EnvConfiguration::Custom:
         return "custom";
   }

   assert(false);
   return "unknown";
}

std::string ApplicationSettings::networkName(NetworkType type)
{
   switch (type) {
      case NetworkType::MainNet:
         return "mainnet";
      case NetworkType::TestNet:
         return "testnet";
      case NetworkType::RegTest:
         return "regtest";
      default:
         assert(false);
         return "unknown";
   }
}
