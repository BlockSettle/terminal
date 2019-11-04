#ifndef __APPLICATION_SETTINGS_H__
#define __APPLICATION_SETTINGS_H__

#include <atomic>
#include <QStringList>
#include <QString>
#include <QSettings>

#include <bdmenums.h>

#include "ArmorySettings.h"
#include "autheid_utils.h"
#include "LogManager.h"

// hasher to allow compile std::unordered_map with enum as key
struct EnumClassHash
{
    template <typename T>
    std::size_t operator()(T t) const
    {
        return static_cast<std::size_t>(t);
    }
};



class ApplicationSettings : public QObject
{
   Q_OBJECT
public:
   ApplicationSettings(const QString &appName = QLatin1String("BlockSettle Terminal")
      , const QString& rootDir = {});
   ~ApplicationSettings() noexcept = default;

   ApplicationSettings(const ApplicationSettings&) = delete;
   ApplicationSettings& operator = (const ApplicationSettings&) = delete;

   ApplicationSettings(ApplicationSettings&&) = delete;
   ApplicationSettings& operator = (ApplicationSettings&&) = delete;

   bool LoadApplicationSettings(const QStringList& argList);
   QString  ErrorText() const { return errorText_; }

   void     SaveSettings();

   enum EnvConfiguration
   {
      PROD,
      UAT,
      Staging,
      Custom,
   };
   Q_ENUM(EnvConfiguration)

   enum Setting {
      initialized,
      runArmoryLocally,
      netType,
      armoryDbName,
      armoryDbIp,
      armoryDbPort,
      armoryPathName,
      customPubBridgeHost,
      customPubBridgePort,
      pubBridgePubKey,
      envConfiguration,
      mdServerHost,
      mdServerPort,
      mdhsHost,
      mdhsPort,
      chatServerHost,
      chatServerPort,
      chatServerPubKey,
      chatPrivKey,
      chatPubKey,
      chatDbFile,
      celerUsername,
      localSignerPort,
      signerIndex,
      signerOfflineDir,
      autoSignSpendLimit,
      launchToTray,
      minimizeToTray,
      closeToTray,
      notifyOnTX,
      defaultAuthAddr,
      bsPublicKey,
      logDefault,
      logMessages,
      txCacheFileName,
      nbBackupFilesKeep,
      aqScripts,
      lastAqScript,
      dropQN,
      GUI_main_geometry,
      GUI_main_tab,
      Filter_MD_RFQ,
      Filter_MD_RFQ_Portfolio,
      Filter_MD_QN,
      Filter_MD_QN_cnt,
      ChangeLog_Base_Url,
      Binaries_Dl_Url,
      ResetPassword_Url,
      GetAccount_Url,
      GettingStartedGuide_Url,
      WalletFiltering,
      FxRfqLimit,
      XbtRfqLimit,
      PmRfqLimit,
      DisableBlueDotOnTabOfRfqBlotter,
      PriceUpdateInterval,
      ShowQuoted,
      AdvancedTxDialogByDefault,
      TransactionFilter,
      SubscribeToMDOnStart,
      MDLicenseAccepted,
      authPrivKey,
      jwtUsername,
      zmqLocalSignerPubKeyFilePath,
      remoteSigners,
      rememberLoginUserName,
      armoryServers,
      defaultArmoryServersKeys,
      twoWaySignerAuth,
      ChartProduct,
      ChartTimeframe,
      ChartCandleCount,
      LastAqDir,
      proxyServerPubKey,
      _last
   };

   struct SettingDef {
      QString  path;
      QVariant defVal;
      mutable bool     read;
      mutable QVariant value;

      SettingDef(const QString &_path, const QVariant &_defVal=QVariant())
         : path(_path), defVal(_defVal), read(false) {}
   };

   QVariant get(Setting s, bool getDefaultValue = false) const;
   bool isDefault(Setting s) const;
   template <typename T> T get(Setting s, bool getDefaultValue = false) const;
   void set(Setting s, const QVariant &val, bool toFile=true);
   void reset(Setting s, bool toFile=true);     // Reset setting to default value

   using State = std::unordered_map<Setting, QVariant, EnumClassHash>;
   State getState() const;
   void setState(const State &);

   void SetDefaultSettings(bool toFile=false);                   // reset all settings to default

   static int GetDefaultArmoryLocalPort(NetworkType networkType);
   static int GetDefaultArmoryRemotePort(NetworkType networkType);
   int GetArmoryRemotePort(NetworkType networkType = NetworkType::Invalid) const;

   static QString localSignerDefaultName();

   QString GetSettingsPath() const;

   QString  GetHomeDir() const;
   QString  GetBackupDir() const;

   SocketType  GetArmorySocketType() const;
   QString  GetDBDir() const;
   QString  GetBitcoinBlocksDir() const;

   QString GetDefaultHomeDir() const;
   QString GetDefaultBitcoinsDir() const;
   QString GetDefaultDBDir() const;

   std::vector<bs::LogConfig> GetLogsConfig() const;

   unsigned int GetWalletScanIndex(const std::string &id) const;
   void SetWalletScanIndex(const std::string &id, unsigned int index);
   std::vector<std::pair<std::string, unsigned int>> UnfinishedWalletsRescan();

   std::pair<autheid::PrivateKey, autheid::PublicKey> GetAuthKeys();

   std::string pubBridgeHost() const;
   std::string pubBridgePort() const;

   void selectNetwork();

   bool isAutheidTestEnv() const;

   // Returns "prod", "uat", "staging" or "custom"
   static std::string envName(EnvConfiguration conf);

   // Returns "mainnet", "testnet" or "regtest"
   static std::string networkName(NetworkType type);

   QString ccFilePath() const;

signals:
   void settingChanged(int setting, QVariant value);

private:

   void SetHomeDir(const QString& path);
   void SetBitcoinsDir(const QString& path);
   void SetDBDir(const QString& path);

   QString AppendToWritableDir(const QString &filename) const;
   bs::LogConfig parseLogConfig(const QStringList &) const;
   bs::LogLevel parseLogLevel(QString) const;

private:
   QSettings   settings_;
   std::map<Setting, SettingDef> settingDefs_;
   mutable std::atomic_flag lock_ = ATOMIC_FLAG_INIT;

   QString  errorText_;
   QString  commonRoot_;
   QString  dataDir_;
   QString  bitcoinsDir_;
   QString  dbDir_;

   autheid::PrivateKey  authPrivKey_;
   autheid::PublicKey   authPubKey_;
};

template<> QString ApplicationSettings::get<QString>(Setting s, bool getDefaultValue) const;
template<> std::string ApplicationSettings::get<std::string>(Setting set, bool getDefaultValue) const;
template<> bool ApplicationSettings::get<bool>(Setting set, bool getDefaultValue) const;
template<> int ApplicationSettings::get<int>(Setting set, bool getDefaultValue) const;
template<> unsigned int ApplicationSettings::get<unsigned int>(Setting set, bool getDefaultValue) const;
template<> double ApplicationSettings::get<double>(Setting set, bool getDefaultValue) const;
template<> QRect ApplicationSettings::get<QRect>(Setting set, bool getDefaultValue) const;
template<> QStringList ApplicationSettings::get<QStringList>(Setting set, bool getDefaultValue) const;
template<> QVariantMap ApplicationSettings::get<QVariantMap> (Setting set, bool getDefaultValue) const;
template<> NetworkType ApplicationSettings::get<NetworkType>(Setting set, bool getDefaultValue) const;

#endif // __APPLICATION_SETTINGS_H__
