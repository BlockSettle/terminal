#include <QCommandLineParser>
#include <QDir>
#include <QSettings>
#include <QStandardPaths>
#include <QMetaEnum>
#include "BIP150_151.h"
#include "BtcDefinitions.h"
#include "BlockDataManagerConfig.h"
#include "BtcUtils.h"
#include "SignerSettings.h"
#include "SystemFileUtils.h"

static const QString listenName = QString::fromStdString("listen");
static const QString listenHelp = QObject::tr("IP address to listen on");

static const QString portName = QString::fromStdString("port");
static const QString portHelp = QObject::tr("Specify command port number");

static const QString logName = QString::fromStdString("log");
static const QString logHelp = QObject::tr("Log file name (relative to temp dir)");

//static const QString walletsDirName = QString::fromStdString("dirwallets");
//static const QString walletsDirHelp = QObject::tr("Directory where wallets reside");

static const QString testnetName = QString::fromStdString("testnet");
static const QString testnetHelp = QObject::tr("Set bitcoin network type to testnet");

static const QString mainnetName = QString::fromStdString("mainnet");
static const QString mainnetHelp = QObject::tr("Set bitcoin network type to mainnet");

//static const QString signName = QString::fromStdString("sign");
//static const QString signHelp = QObject::tr("Sign transaction[s] from request file - auto toggles offline mode (headless mode only)");

static const QString runModeName = QString::fromStdString("guimode");
static QString runModeHelp = QObject::tr("GUI run mode [fullgui|lightgui]");

static const QString srvIDKeyName = QString::fromStdString("server_id_key");
static QString srvIDKeyHelp = QObject::tr("The server's compressed BIP 150 ID key (hex)");

static const QString autoSignLimitName = QString::fromStdString("auto_sign_spend_limit");
static const QString autoSignLimitHelp = QObject::tr("Spend limit expressed in XBT for auto-sign operations");

static const QString woName = QString::fromStdString("watchonly");
static const QString woHelp = QObject::tr("Try to load only watching-only wallets");

static const QString closeHeadlessName = QString::fromStdString("close_headless");
static const QString closeHeadlessHelp = QString::fromStdString("Shutdown headless process after signer GUI exit");

SignerSettings::SignerSettings(const QString &fileName)
   : QObject(nullptr)
{
   writableDir_ = SystemFilePaths::appDataLocation();
   backend_ = std::make_shared<QSettings>(QString::fromStdString(writableDir_ + "/") + fileName, QSettings::IniFormat);

   settingDefs_ = {
      { OfflineMode,       SettingDef(QStringLiteral("Offline"), false)},
      { WatchingOnly,      SettingDef(QStringLiteral("WatchingOnly"), false)},
      { TestNet,           SettingDef(QStringLiteral("TestNet"), false) },
      //{ WalletsDir,        SettingDef(QStringLiteral("WalletsDir")) },
      { AutoSignWallet,    SettingDef(QStringLiteral("AutoSignWallet")) },
      { LogFileName,       SettingDef(QStringLiteral("LogFileName"), QString::fromStdString(writableDir_ + "/bs_gui_signer.log")) },
      { ListenAddress,     SettingDef(QStringLiteral("ListenAddress"), QStringLiteral("0.0.0.0")) },
      { ListenPort,        SettingDef(QStringLiteral("ListenPort"), 23456) },
      { ServerIDKeyStr,    SettingDef(QStringLiteral("ServerIDKeyStr")) },
      { LimitManualXBT,    SettingDef(QStringLiteral("Limits/Manual/XBT"), (qint64)UINT64_MAX) },
      { LimitAutoSignXBT,  SettingDef(QStringLiteral("Limits/AutoSign/XBT"), (qint64)UINT64_MAX) },
      { LimitAutoSignTime, SettingDef(QStringLiteral("Limits/AutoSign/Time"), 3600) },
      { LimitManualPwKeep, SettingDef(QStringLiteral("Limits/Manual/PasswordInMemKeepInterval"), 0) },
      { HideEidInfoBox,    SettingDef(QStringLiteral("HideEidInfoBox"), 0) },
      { TrustedTerminals,  SettingDef(QStringLiteral("TrustedTerminals")) },
      { TwoWayAuth,        SettingDef(QStringLiteral("TwoWayAuth"), true) }
   };
}

QString SignerSettings::getExportWalletsDir() const
{
   QString result = get(ExportWalletsDir).toString();
   if (!result.isEmpty()) {
      return result;
   }
   return dirDocuments();
}

QVariant SignerSettings::get(Setting set) const
{
   auto itSD = settingDefs_.find(set);
   if (itSD == settingDefs_.end()) {
      return QVariant{};
   }

   if (itSD->second.read) {   // lazy init
      return itSD->second.value;
   }

   if (itSD->second.path.isEmpty()) {
      itSD->second.value = itSD->second.defVal;
   }
   else {
      itSD->second.value = backend_->value(itSD->second.path, itSD->second.defVal);
   }

   itSD->second.read = true;
   return itSD->second.value;
}

void SignerSettings::set(Setting s, const QVariant &val, bool toFile)
{
   if (val.isValid()) {
      auto itSD = settingDefs_.find(s);

      if (itSD != settingDefs_.end()) {
         itSD->second.read = true;
         if (val != itSD->second.value) {
            itSD->second.value = val;
            if (toFile && !itSD->second.path.isEmpty()) {
               backend_->setValue(itSD->second.path, val);
            }
            settingChanged(s, val);
         }
      }
   }
}

void SignerSettings::reset(Setting s, bool toFile)
{
   auto itSD = settingDefs_.find(s);

   if (itSD != settingDefs_.end()) {
      itSD->second.read = true;
      if (itSD->second.value != itSD->second.defVal) {
         itSD->second.value = itSD->second.defVal;
         if (toFile && !itSD->second.path.isEmpty()) {
            backend_->setValue(itSD->second.path, itSD->second.value);
         }
         emit settingChanged(s, itSD->second.defVal);
      }
   }
}

void SignerSettings::settingChanged(Setting s, const QVariant &)
{
   switch (s) {
   case OfflineMode:
      emit offlineChanged();
      break;
   case TestNet:
      emit testNetChanged();
      break;
   case WatchingOnly:
      emit woChanged();
      break;
   case AutoSignWallet:
      emit autoSignWalletChanged();
      break;
   case ListenAddress:
   case ListenPort:
      emit listenSocketChanged();
      break;
   case LimitManualXBT:
      emit limitManualXbtChanged();
      break;
   case LimitAutoSignXBT:
      emit limitAutoSignXbtChanged();
      break;
   case LimitAutoSignTime:
      emit limitAutoSignTimeChanged();
      break;
   case LimitManualPwKeep:
      emit limitManualPwKeepChanged();
      break;
   case HideEidInfoBox:
      emit hideEidInfoBoxChanged();
      break;
   case TrustedTerminals:
      emit trustedTerminalsChanged();
      break;
   case TwoWayAuth:
      emit twoWayAuthChanged();
      break;
   default: break;
   }
}

// Get the server BIP 150 ID key. Intended only for when the key is passed in
// via a CL arg.
//
// INPUT:  N/A
// OUTPUT: A buffer containing the binary key. (BinaryData)
// RETURN: True is success, false if failure.
bool SignerSettings::getSrvIDKeyBin(BinaryData& keyBuf)
{
   if (!verifyServerIDKey()) {
      return false;
   }

   // Make sure the key is a valid public key.
   keyBuf.resize(BIP151PUBKEYSIZE);
   keyBuf = READHEX(serverIDKeyStr().toStdString());

   return true;
}

bool SignerSettings::loadSettings(const QStringList &args)
{
   QMetaEnum runModesEnum = QMetaEnum::fromType<bs::signer::ui::RunMode>();

   QCommandLineParser parser;
   parser.setApplicationDescription(QObject::tr("BlockSettle Signer"));
   parser.addHelpOption();
   parser.addOption({ listenName, listenHelp, QObject::tr("ip/host") });
   parser.addOption({ portName, portHelp, QObject::tr("port") });
   parser.addOption({ logName, logHelp, QObject::tr("log") });
   //parser.addOption({ walletsDirName, walletsDirHelp, QObject::tr("dir") });
   parser.addOption({ testnetName, testnetHelp });
   parser.addOption({ mainnetName, mainnetHelp });
   parser.addOption({ runModeName, runModeHelp, runModeName });
   parser.addOption({ srvIDKeyName, srvIDKeyHelp, srvIDKeyName });
   parser.addOption({ autoSignLimitName, autoSignLimitHelp, QObject::tr("limit") });
   //parser.addOption({ signName, signHelp, QObject::tr("filename") });
   parser.addOption({ woName, woHelp });
   parser.addOption({ closeHeadlessName, closeHeadlessHelp, QLatin1String("true") });

   parser.process(args);

   if (parser.isSet(listenName)) {
      set(ListenAddress, parser.value(listenName), false);
   }

   if (parser.isSet(portName)) {
      set(ListenPort, parser.value(portName), false);
   }

   if (parser.isSet(logName)) {
      set(LogFileName, QString::fromStdString(writableDir_ + "/") + parser.value(logName), false);
   }

   if (parser.isSet(mainnetName)) {
      set(TestNet, false, false);
   }
   else if (parser.isSet(testnetName)) {
      set(TestNet, true, false);
   }

   if (parser.isSet(woName)) {
      set(WatchingOnly, true, false);
   }

   if (parser.isSet(autoSignLimitName)) {
      const auto val = parser.value(autoSignLimitName).toDouble();
      if (val > 0) {
         set(LimitAutoSignXBT, (qulonglong)(val * BTCNumericTypes::BalanceDivider), false);
      }
   }

   if (parser.isSet(runModeName)) {
      int runModeValue = runModesEnum.keyToValue(parser.value(runModeName).toLatin1());
      if (runModeValue < 0) {
         return false;
      }
      runMode_ = static_cast<bs::signer::ui::RunMode>(runModeValue);
      if (runMode_ != bs::signer::ui::RunMode::fullgui && runMode_ != bs::signer::ui::RunMode::lightgui) {
         return false;
      }
   } else {
      runMode_ = bs::signer::ui::RunMode::fullgui;
   }

   if (parser.isSet(srvIDKeyName)) {
      set(ServerIDKeyStr, parser.value(srvIDKeyName), false);
   }

//   if (parser.isSet(signName)) {
//      if (!parser.isSet(headlessName)) {
//         throw std::logic_error("Batch offline signing is possible only in headless mode");
//      }
//      reqFiles_ << parser.value(signName);
//      set(OfflineMode, true, false);
//   }

   NetworkConfig config;
   if (testNet()) {
      config.selectNetwork(NETWORK_MODE_TESTNET);
   }
   else {
      config.selectNetwork(NETWORK_MODE_MAINNET);
   }

   if (parser.isSet(closeHeadlessName)) {
      closeHeadless_ = QVariant::fromValue(parser.value(closeHeadlessName)).toBool();
   }

   return true;
}

bs::signer::Limits SignerSettings::limits() const
{
   return bs::signer::Limits {
      (uint64_t)get(LimitAutoSignXBT).toULongLong(),
      (uint64_t)get(LimitManualXBT).toULongLong(),
      get(LimitAutoSignTime).toInt(),
      get(LimitManualPwKeep).toInt()
   };
}

QString SignerSettings::dirDocuments() const
{
   return QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
}

//void SignerSettings::setServerIDKeyStr(const QString& inKeyStr)
//{
//   if (inKeyStr == get(ServerIDKeyStr).toString()) {
//      return;
//   }
//   set(ServerIDKeyStr, inKeyStr);
//}

void SignerSettings::setExportWalletsDir(const QString &val)
{
#if defined (Q_OS_WIN)
   set(ExportWalletsDir, val);
#else
   const auto dir = val.startsWith(QLatin1Char('/')) ? val : QLatin1String("/") + val;
   set(ExportWalletsDir, dir);
#endif
}

void SignerSettings::setXbtLimit(const double val, Setting s)
{
   uint64_t limit = UINT64_MAX;
   if (val > 0) {
      limit = val * BTCNumericTypes::BalanceDivider;
   }
   set(s, (qulonglong)limit);
}

QString SignerSettings::secondsToIntervalStr(int s)
{
   QString result;
   if (s <= 0) {
      return result;
   }
   if (s >= 3600) {
      int h = std::floor(s / 3600);
      s -= h * 3600;
      result = QString::number(h) + QLatin1String("h");
   }
   if (s >= 60) {
      int m = std::floor(s / 60);
      s -= m * 60;
      if (!result.isEmpty()) {
         result += QLatin1String(" ");
      }
      result += QString::number(m) + QLatin1String("m");
   }
   if (!result.isEmpty()) {
      if (s <= 0) {
         return result;
      }
      result += QLatin1String(" ");
   }
   result += QString::number(s) + QLatin1String("s");
   return result;
}

int SignerSettings::intervalStrToSeconds(const QString &s)
{
   int result = 0;
   const auto elems = s.split(QRegExp(QLatin1String("[ ,+]+"), Qt::CaseInsensitive), QString::SkipEmptyParts);
   const QRegExp reElem(QLatin1String("(\\d+)([a-z]*)"), Qt::CaseInsensitive);
   for (const auto &elem : elems) {
      const auto pos = reElem.indexIn(elem);
      if (pos < 0) {
         continue;
      }
      const int val = reElem.cap(1).toInt();
      if (val <= 0) {
         continue;
      }
      const auto suffix = reElem.cap(2).toLower();
      if (suffix.isEmpty() || (suffix == QLatin1String("s")) || (suffix == QLatin1String("sec")
         || suffix.startsWith(QLatin1String("second")))) {
         result += val;
      }
      else if ((suffix == QLatin1String("m")) || (suffix == QLatin1String("min"))
         || suffix.startsWith(QLatin1String("minute"))) {
         result += val * 60;
      }
      else if ((suffix == QLatin1String("h")) || suffix.startsWith(QLatin1String("hour"))) {
         result += val * 3600;
      }
   }
   return result;
}

// Get the server BIP 150 ID key. Intended only for when the key is passed in
// via a CL arg.
//
// INPUT:  N/A
// OUTPUT: A buffer containing the binary key. (BinaryData)
// RETURN: True is success, false if failure.
bool SignerSettings::verifyServerIDKey()
{
   BinaryData keyBuf(BIP151PUBKEYSIZE);
   if (serverIDKeyStr().toStdString().empty()) {
      return false;
   }

   // Make sure the key is a valid public key.
   keyBuf = READHEX(serverIDKeyStr().toStdString());
   if (keyBuf.getSize() != BIP151PUBKEYSIZE) {
      return false;
   }
   if (!(CryptoECDSA().VerifyPublicKeyValid(keyBuf))) {
      return false;
   }

   return true;
}
