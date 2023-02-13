/*

***********************************************************************************
* Copyright (C) 2020 - 2021, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include <QCommandLineParser>
#include <QDir>
#include <QStandardPaths>
#include <QMetaEnum>
#include <QMessageBox>
#include "ArmoryConfig.h"
#include "BIP150_151.h"
#include "BtcDefinitions.h"
#include "BtcUtils.h"
#include "SignerSettings.h"
#include "SystemFileUtils.h"
#include "bs_signer.pb.h"
#include "HeadlessSettings.h"
using namespace Blocksettle::Communication;

static const QString testnetName = QString::fromStdString("testnet");
static const QString testnetHelp = QObject::tr("Set bitcoin network type to testnet");

static const QString runModeName = QString::fromStdString("guimode");
static QString runModeHelp = QObject::tr("GUI run mode [fullgui|litegui]");

static const QString srvIDKeyName = QString::fromStdString("server_id_key");
static QString srvIDKeyHelp = QObject::tr("The server's compressed BIP 150 ID key (hex)");

static const QString portName = QString::fromStdString("port");
static const QString portHelp = QObject::tr("Local TCP connection port to signer process");

namespace {

double convertFromSatoshi(uint64_t satoshi)
{
   if (satoshi == UINT64_MAX) {
      return 0.0;
   }
   return double(satoshi) / double(BTCNumericTypes::BalanceDivider);
}

uint64_t convertToSatoshi(double xbt)
{
   if (xbt == 0.0) {
      return UINT64_MAX;
   }
   return  uint64_t(std::llround(xbt * BTCNumericTypes::BalanceDivider));
}

} // namespace

SignerSettings::SignerSettings()
   : QObject(nullptr)
   , writableDir_(SystemFilePaths::appDataLocation())
   , fileName_(writableDir_ + "/signer.json")
   , d_(new Settings)
{
   HeadlessSettings::loadSettings(d_.get(), fileName_);
}

SignerSettings::~SignerSettings() = default;

void SignerSettings::settingChanged(int setting)
{
   switch (static_cast<signer::Setting>(setting)) {
   case signer::OfflineMode:
      emit offlineChanged();
      break;
   case signer::TestNet:
      emit testNetChanged();
      break;
   case signer::WatchingOnly:
      emit woChanged();
      break;
   case signer::ExportWalletsDir:
      emit exportWalletsDirChanged();
      break;
   case signer::AutoSignWallet:
      emit autoSignWalletChanged();
      break;
   case signer::ListenAddress:
   case signer::ListenPort:
   case signer::AcceptFrom:
      emit listenSocketChanged();
      break;
   case signer::LimitManualXBT:
      emit limitManualXbtChanged();
      break;
   case signer::LimitAutoSignXBT:
      emit limitAutoSignXbtChanged();
      break;
   case signer::LimitAutoSignTime:
      emit limitAutoSignTimeChanged();
      break;
   case signer::LimitManualPwKeep:
      emit limitManualPwKeepChanged();
      break;
   case signer::HideEidInfoBox:
      emit hideEidInfoBoxChanged();
      break;
   case signer::TrustedTerminals:
      emit trustedTerminalsChanged();
      break;
   case signer::TwoWaySignerAuth:
      emit twoWaySignerAuthChanged();
      break;
   default:
      break;
   }

   emit changed(setting);

   HeadlessSettings::saveSettings(*d_, fileName_);
}

// Get the server BIP 150 ID key. Intended only for when the key is passed in
// via a CL arg.
//
// INPUT:  N/A
// OUTPUT: A buffer containing the binary key. (BinaryData)
// RETURN: True is success, false if failure.
bool SignerSettings::getSrvIDKeyBin(BinaryData& keyBuf)
{
   // READHEX below must succeed because verifyServerIDKey checks this
   if (!verifyServerIDKey()) {
      return false;
   }

   // Make sure the key is a valid public key.
   keyBuf = READHEX(serverIDKeyStr().toStdString());

   return true;
}

QString SignerSettings::getExportWalletsDir() const
{
   return QString::fromStdString(d_->export_wallets_dir());
}

QString SignerSettings::autoSignWallet() const
{
   return QString::fromStdString(d_->auto_sign_wallet());
}

bool SignerSettings::offline() const
{
   return d_->offline();
}

double SignerSettings::limitManualXbt() const
{
   return convertFromSatoshi(d_->limit_manual_xbt());
}

double SignerSettings::limitAutoSignXbt() const
{
   return convertFromSatoshi(d_->limit_auto_sign_xbt());
}

bool SignerSettings::autoSignUnlimited() const
{
   return (d_->limit_auto_sign_xbt() == UINT64_MAX);
}

bool SignerSettings::manualSignUnlimited() const
{
   return (d_->limit_manual_xbt() == UINT64_MAX);
}

int SignerSettings::limitAutoSignTime() const
{
   return d_->limit_auto_sign_time();
}

QString SignerSettings::limitAutoSignTimeStr() const
{
   return secondsToIntervalStr(d_->limit_auto_sign_time());
}

QString SignerSettings::limitManualPwKeepStr() const
{
   return secondsToIntervalStr(d_->limit_pass_keep_time());
}

bool SignerSettings::loadSettings(const std::shared_ptr<HeadlessSettings> &mainSettings)
{
   if (!mainSettings) {
      return false;
   }
   srvIDKey_ = mainSettings->serverIdKey().toHexStr();
   signerPort_ = mainSettings->interfacePort();
   d_->set_test_net(mainSettings->testNet());

   if (d_->test_net()) {
      Armory::Config::NetworkSettings::selectNetwork(Armory::Config::NETWORK_MODE_TESTNET);
   }
   else {
      Armory::Config::NetworkSettings::selectNetwork(Armory::Config::NETWORK_MODE_MAINNET);
   }
   return true;
}

QString SignerSettings::serverIDKeyStr() const
{
   return QString::fromStdString(srvIDKey_);
}

QString SignerSettings::listenAddress() const
{
   return QString::fromStdString(d_->listen_address());
}

QString SignerSettings::acceptFrom() const
{
   return QString::fromStdString(d_->accept_from());
}

QString SignerSettings::port() const
{
   if (d_->listen_port() == 0) {
      return QLatin1String("23456");
   }
   return QString::number(d_->listen_port());
}

QString SignerSettings::logFileName() const
{
   return QString::fromStdString(writableDir_ + "/bs_gui_signer.log");
}

bool SignerSettings::testNet() const
{
   return d_->test_net();
}

bool SignerSettings::watchingOnly() const
{
   return d_->watching_only();
}

bs::signer::Limits SignerSettings::limits() const
{
   bs::signer::Limits result;
   result.autoSignSpendXBT = d_->limit_auto_sign_xbt();
   result.manualSpendXBT = d_->limit_manual_xbt();
   result.autoSignTimeS = d_->limit_auto_sign_time();
   result.manualPassKeepInMemS = d_->limit_pass_keep_time();
   return result;
}

bool SignerSettings::hideEidInfoBox() const
{
   return d_->hide_eid_info_box();
}

QStringList SignerSettings::trustedTerminals() const
{
   QStringList result;
   for (const auto& item : d_->trusted_terminals()) {
      result.push_back(QString::fromStdString(item.id() + ":" + item.key()));
   }
   return result;
}

bool SignerSettings::twoWaySignerAuth() const
{
   return d_->two_way_signer_auth() == Blocksettle::Communication::signer::TWO_WAY_AUTH_ENABLED;
}

QString SignerSettings::dirDocuments() const
{
   return QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
}

void SignerSettings::setOffline(bool val)
{
   if (d_->offline() != val) {
      d_->set_offline(val);
      settingChanged(signer::Setting::OfflineMode);
   }
}

void SignerSettings::setTestNet(bool val)
{
   if (d_->test_net() != val) {
      d_->set_test_net(val);
      settingChanged(signer::Setting::TestNet);
   }
}

void SignerSettings::setWatchingOnly(const bool val)
{
   if (d_->watching_only() != val) {
      d_->set_watching_only(val);
      settingChanged(signer::Setting::WatchingOnly);
   }
}

void SignerSettings::setExportWalletsDir(const QString &val)
{
   setStringSetting(val, d_->mutable_export_wallets_dir(), signer::Setting::ExportWalletsDir);
}

void SignerSettings::setAutoSignWallet(const QString &val)
{
   setStringSetting(val, d_->mutable_auto_sign_wallet(), signer::Setting::AutoSignWallet);
}

void SignerSettings::setListenAddress(const QString &val)
{
   setStringSetting(val, d_->mutable_listen_address(), signer::Setting::ListenAddress);
}

void SignerSettings::setAcceptFrom(const QString &val)
{
   setStringSetting(val, d_->mutable_accept_from(), signer::Setting::AcceptFrom);
}

void SignerSettings::setPort(const QString &val)
{
   int valCopy = val.toInt();
   if (valCopy != d_->listen_port()) {
      d_->set_listen_port(valCopy);
      settingChanged(signer::Setting::ListenPort);
   }
}

void SignerSettings::setLimitManualXbt(const double val)
{
   auto valCopy = convertToSatoshi(val);
   if (valCopy != d_->limit_manual_xbt()) {
      d_->set_limit_manual_xbt(valCopy);
      settingChanged(signer::Setting::LimitManualXBT);
   }
}

void SignerSettings::setLimitAutoSignXbt(const double val)
{
   auto valCopy = convertToSatoshi(val);
   if (d_->limit_auto_sign_xbt() != valCopy) {
      d_->set_limit_auto_sign_xbt(valCopy);
      settingChanged(signer::Setting::LimitAutoSignXBT);
   }
}

void SignerSettings::setLimitAutoSignTimeStr(const QString &val)
{
   int valueCopy = intervalStrToSeconds(val);
   if (valueCopy != d_->limit_auto_sign_time()) {
      d_->set_limit_auto_sign_time(valueCopy);
      settingChanged(signer::Setting::LimitAutoSignTime);
   }
}

void SignerSettings::setLimitManualPwKeepStr(const QString &val)
{
   int valueCopy = intervalStrToSeconds(val);
   if (d_->limit_pass_keep_time() != valueCopy) {
      d_->set_limit_pass_keep_time(valueCopy);
      settingChanged(signer::Setting::LimitManualPwKeep);
   }
}

void SignerSettings::setHideEidInfoBox(bool val)
{
   if (val != d_->hide_eid_info_box()) {
      d_->set_hide_eid_info_box(val);
      settingChanged(signer::Setting::HideEidInfoBox);
   }
}

void SignerSettings::setTrustedTerminals(const QStringList &val)
{
   d_->clear_trusted_terminals();
   for (const QString &s : val) {
      if (s.isEmpty()) {
         continue;
      }

      QStringList split = s.split(QLatin1Char(':'));
      if (split.size() != 2) {
         continue;
      }

      auto line = d_->add_trusted_terminals();
      line->set_id(split[0].toStdString());
      line->set_key(split[1].toStdString());
   }
   settingChanged(signer::Setting::TrustedTerminals);
}

void SignerSettings::setTwoWaySignerAuth(bool val)
{
   const bool valOld = (d_->two_way_signer_auth() == Blocksettle::Communication::signer::TWO_WAY_AUTH_ENABLED);
   if (val != valOld) {
      d_->set_two_way_signer_auth(val ? Blocksettle::Communication::signer::TWO_WAY_AUTH_ENABLED
         : Blocksettle::Communication::signer::TWO_WAY_AUTH_DISABLED);
      settingChanged(signer::Setting::TwoWaySignerAuth);
   }
}

QString SignerSettings::secondsToIntervalStr(int s)
{
   QString result;
   if (s <= 0) {
      return result;
   }
   if (s >= 3600) {
      int h = s / 3600;
      s -= h * 3600;
      result = QString::number(h) + QLatin1String("h");
   }
   if (s >= 60) {
      int m = s / 60;
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
   if (serverIDKeyStr().toStdString().empty()) {
      return false;
   }

   // Make sure the key is a valid public key.
   try {
      BinaryData keyBuf = READHEX(serverIDKeyStr().toStdString());
      if (keyBuf.getSize() != BIP151PUBKEYSIZE) {
         return false;
      }
      if (!(CryptoECDSA().VerifyPublicKeyValid(keyBuf))) {
         return false;
      }
   } catch (...) {
      return false;
   }

   return true;
}

void SignerSettings::setStringSetting(const QString &val, std::string *oldValue, int setting)
{
   std::string valueCopy = val.toStdString();
   if (valueCopy != *oldValue) {
      *oldValue = std::move(valueCopy);
      settingChanged(setting);
   }
}
