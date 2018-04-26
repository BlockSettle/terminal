#include <QCommandLineParser>
#include <QDir>
#include <QSettings>
#include <QStandardPaths>
#include "BtcDefinitions.h"
#include "BlockDataManagerConfig.h"
#include "BtcUtils.h"
#include "SignerSettings.h"


static const QString pubKeyName = QString::fromStdString("pubkey");
static const QString pubKeyHelp = QObject::tr("Public key for secured connection");

static const QString privKeyName = QString::fromStdString("privkey");
static const QString privKeyHelp = QObject::tr("Private key for secured connection");

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

static const QString pwhashName = QString::fromStdString("pwhash");
static const QString pwhashHelp = QObject::tr("Password hash (first 8 bytes of SHA256 in hex)");

static const QString signName = QString::fromStdString("sign");
static const QString signHelp = QObject::tr("Sign transaction[s] from request file - auto toggles offline mode (headless mode only)");

static const QString headlessName = QString::fromStdString("headless");
static const QString headlessHelp = QObject::tr("Run without UI");

static const QString autoSignLimitName = QString::fromStdString("auto_sign_spend_limit");
static const QString autoSignLimitHelp = QObject::tr("Spend limit expressed in XBT for auto-sign operations");


SignerSettings::SignerSettings(const QStringList &args, const QString &fileName)
   : QObject(nullptr)
{
   QDir logDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
   logDir.cdUp();
   writableDir_ = logDir.path().toStdString();

   backend_ = std::make_shared<QSettings>(QString::fromStdString(writableDir_ + "/") + fileName, QSettings::IniFormat);

   settingDefs_ = {
      { OfflineMode,       SettingDef(QStringLiteral("Offline"), false)},
      { TestNet,           SettingDef(QStringLiteral("TestNet"), false) },
      { WalletsDir,        SettingDef(QStringLiteral("WalletsDir")) },
      { AutoSignWallet,    SettingDef(QStringLiteral("AutoSignWallet")) },
      { LogFileName,       SettingDef(QStringLiteral("LogFileName"), QString::fromStdString(writableDir_ + "/bs_signer.log")) },
      { ListenAddress,     SettingDef(QStringLiteral("ListenAddress"), QStringLiteral("0.0.0.0")) },
      { ListenPort,        SettingDef(QStringLiteral("ListenPort"), 23456) },
      { ConnPubKey,        SettingDef(QStringLiteral("ConnPubKey"), QStringLiteral("t>ituO$mt-[Fl}&IE%EicU@L&LvC%8i$$nS3YFm}")) },
      { ConnPrivKey,       SettingDef(QStringLiteral("ConnPrivKey"), QStringLiteral("bjPNYGx5q<?O:Nc2h[LDoKk&&Ydxuq-[onOF-=Nw")) },
      { PasswordHash,      SettingDef(QStringLiteral("PasswordHash")) },
      { LimitManualXBT,    SettingDef(QStringLiteral("Limits/Manual/XBT"), (qint64)UINT64_MAX) },
      { LimitAutoSignXBT,  SettingDef(QStringLiteral("Limits/AutoSign/XBT"), (qint64)UINT64_MAX) },
      { LimitAutoSignTime, SettingDef(QStringLiteral("Limits/AutoSign/Time"), 3600) },
      { LimitManualPwKeep, SettingDef(QStringLiteral("Limits/Manual/PasswordInMemKeepInterval"), 0) }
   };
   parseArguments(args);
}

static const QString testnetSubdir = QLatin1String("testnet3");
#if defined (Q_OS_WIN)
static const QString appDirName = QLatin1String("Blocksettle");
#elif defined (Q_OS_OSX)
static const QString appDirName = QLatin1String("Blocksettle");
#elif defined (Q_OS_LINUX)
static const QString appDirName = QLatin1String(".blocksettle");
#endif

QString SignerSettings::getWalletsDir() const
{
   QString result = get(WalletsDir).toString();
   if (!result.isEmpty()) {
      return result;
   }

   const auto dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
   const auto commonRoot = dir + QDir::separator() + QLatin1String("..") + QDir::separator()
      + QLatin1String("..") + QDir::separator() + appDirName;

   if (testNet()) {
      result = commonRoot + QDir::separator() + testnetSubdir;
   }
   else {
      result = commonRoot;
   }
   result += QDir::separator() + QLatin1String("signer");
   return QDir::cleanPath(result);
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
      emit walletsDirChanged();
      break;
   case WalletsDir:
      emit walletsDirChanged();
      break;
   case AutoSignWallet:
      emit autoSignWalletChanged();
      break;
   case ListenAddress:
   case ListenPort:
      emit listenSocketChanged();
      break;
   case PasswordHash:
      emit passwordChanged();
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
   default: break;
   }
}

void SignerSettings::parseArguments(const QStringList &args)
{
   QCommandLineParser parser;
   parser.setApplicationDescription(QObject::tr("BlockSettle Signer"));
   parser.addHelpOption();
   parser.addOption({ listenName, listenHelp, QObject::tr("ip/host") });
   parser.addOption({ portName, portHelp, QObject::tr("port") });
   parser.addOption({ pubKeyName, pubKeyHelp, QObject::tr("key") });
   parser.addOption({ privKeyName, privKeyHelp, QObject::tr("key") });
   parser.addOption({ pwhashName, pwhashHelp, QObject::tr("hash") });
   parser.addOption({ logName, logHelp, QObject::tr("log") });
   parser.addOption({ walletsDirName, walletsDirHelp, QObject::tr("dir") });
   parser.addOption({ testnetName, testnetHelp });
   parser.addOption({ autoSignLimitName, autoSignLimitHelp, QObject::tr("limit") });
   parser.addOption({ signName, signHelp, QObject::tr("filename") });
   parser.addOption({ headlessName, headlessHelp });

   parser.process(args);

   if (parser.isSet(listenName)) {
      set(ListenAddress, parser.value(listenName), false);
   }

   if (parser.isSet(portName)) {
      set(ListenPort, parser.value(portName), false);
   }

   if (parser.isSet(pubKeyName)) {
      if (parser.value(pubKeyName).length() != 40) {
         throw std::runtime_error("invalid pubkey size");
      }
      set(ConnPubKey, parser.value(pubKeyName), false);
   }

   if (parser.isSet(privKeyName)) {
      if (parser.value(privKeyName).length() != 40) {
         throw std::runtime_error("invalid privkey size");
      }
      set(ConnPrivKey, parser.value(privKeyName), false);
   }

   if (parser.isSet(pwhashName)) {
      set(PasswordHash, parser.value(pwhashName), false);
   }

   if (parser.isSet(logName)) {
      set(LogFileName, QString::fromStdString(writableDir_ + "/") + parser.value(logName), false);
   }

   if (parser.isSet(walletsDirName)) {
      set(WalletsDir, parser.value(walletsDirName), false);
   }

   if (parser.isSet(testnetName)) {
      set(TestNet, true, false);
   }

   if (parser.isSet(autoSignLimitName)) {
      const auto val = parser.value(autoSignLimitName).toDouble();
      if (val > 0) {
         set(LimitAutoSignXBT, (qulonglong)(val * BTCNumericTypes::BalanceDivider), false);
      }
   }

   if (parser.isSet(signName)) {
      if (!parser.isSet(headlessName)) {
         throw std::logic_error("Batch offline signing is possible only in headless mode");
      }
      reqFiles_ << parser.value(signName);
      set(OfflineMode, true, false);
   }

   BlockDataManagerConfig config;
   if (testNet()) {
      config.selectNetwork("Test");
   }
   else {
      config.selectNetwork("Main");
   }
}

SignContainer::Limits SignerSettings::limits() const
{
   return SignContainer::Limits {
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

void SignerSettings::setPassword(const QString &password)
{
   if (password == get(PasswordHash).toString()) {
      return;
   }
   const auto hash = BtcUtils::getSha256(password.toStdString());
   set(PasswordHash, QString::fromStdString(hash.getSliceCopy(0, 8).toHexStr()));
}

void SignerSettings::setWalletsDir(const QString &val)
{
#if defined (Q_OS_WIN)
   set(WalletsDir, val);
#else
   const auto dir = val.startsWith(QLatin1Char('/')) ? val : QLatin1String("/") + val;
   set(WalletsDir, dir);
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
