/*

***********************************************************************************
* Copyright (C) 2019 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "AutoSignQuoteProvider.h"

#include "HeadlessContainer.h"
#include "WalletManager.h"
#include "Wallets/SyncWalletsManager.h"
#include "Wallets/SyncHDWallet.h"
#include "UserScriptRunner.h"

#include <Celer/CelerClient.h>
#include <QCoreApplication>
#include <QFileInfo>
#include <QDir>


AutoSignScriptProvider::AutoSignScriptProvider(const std::shared_ptr<spdlog::logger> &logger
   , UserScriptRunner *scriptRunner
   , const std::shared_ptr<ApplicationSettings> &appSettings
   , const std::shared_ptr<HeadlessContainer> &container
   , const std::shared_ptr<CelerClientQt> &celerClient
   , QObject *parent)
   : QObject(parent), logger_(logger), scriptRunner_(scriptRunner)
   , appSettings_(appSettings)
   , signingContainer_(container)
   , celerClient_(celerClient)
{
   scriptRunner_->setParent(this);

   if (walletsManager_) {
      scriptRunner_->setWalletsManager(walletsManager_);
   }

   if (signingContainer_) {
      const auto hct = dynamic_cast<QtHCT*>(signingContainer_->cbTarget());
      if (hct) {
         connect(hct, &QtHCT::ready, this, &AutoSignScriptProvider::onSignerStateUpdated, Qt::QueuedConnection);
         connect(hct, &QtHCT::disconnected, this, &AutoSignScriptProvider::onSignerStateUpdated, Qt::QueuedConnection);
         connect(hct, &QtHCT::AutoSignStateChanged, this, &AutoSignScriptProvider::onAutoSignStateChanged);
      }
   }

   connect(scriptRunner_, &UserScriptRunner::scriptLoaded, this, &AutoSignScriptProvider::onScriptLoaded);
   connect(scriptRunner_, &UserScriptRunner::failedToLoad, this, &AutoSignScriptProvider::onScriptFailed);

   onSignerStateUpdated();

   connect(celerClient_.get(), &CelerClientQt::OnConnectedToServer, this, &AutoSignScriptProvider::onConnectedToCeler);
   connect(celerClient_.get(), &CelerClientQt::OnConnectionClosed, this, &AutoSignScriptProvider::onDisconnectedFromCeler);
}

void AutoSignScriptProvider::onSignerStateUpdated()
{
   disableAutoSign();
   scriptRunner_->disable();

   emit autoSignQuoteAvailabilityChanged();
}

void AutoSignScriptProvider::disableAutoSign()
{
   if (!walletsManager_) {
      return;
   }

   const auto wallet = walletsManager_->getPrimaryWallet();
   if (!wallet) {
      logger_->error("Failed to obtain auto-sign primary wallet");
      return;
   }

   QVariantMap data;
   data[QLatin1String("rootId")] = QString::fromStdString(wallet->walletId());
   data[QLatin1String("enable")] = false;
   signingContainer_->customDialogRequest(bs::signer::ui::GeneralDialogType::ActivateAutoSign, data);
}

void AutoSignScriptProvider::tryEnableAutoSign()
{
   if (!walletsManager_ || !signingContainer_) {
      return;
   }

   const auto wallet = walletsManager_->getPrimaryWallet();
   if (!wallet) {
      logger_->error("Failed to obtain auto-sign primary wallet");
      return;
   }

   QVariantMap data;
   data[QLatin1String("rootId")] = QString::fromStdString(wallet->walletId());
   data[QLatin1String("enable")] = true;
   signingContainer_->customDialogRequest(bs::signer::ui::GeneralDialogType::ActivateAutoSign, data);
}

bool AutoSignScriptProvider::isReady() const
{
   logger_->debug("[{}] signCont: {}, walletsMgr: {}, celer: {}", __func__
      , signingContainer_ && !signingContainer_->isOffline()
      , walletsManager_ && walletsManager_->isReadyForTrading(), celerClient_->IsConnected());
   return signingContainer_ && !signingContainer_->isOffline()
      && walletsManager_ && walletsManager_->isReadyForTrading()
      && celerClient_->IsConnected();
}

void AutoSignScriptProvider::onAutoSignStateChanged(bs::error::ErrorCode result
   , const std::string &walletId)
{
   autoSignState_ = result;
   autoSignWalletId_ = QString::fromStdString(walletId);
   emit autoSignStateChanged();
}

void AutoSignScriptProvider::setScriptLoaded(bool loaded)
{
   scriptLoaded_ = loaded;
   if (!loaded) {
      scriptRunner_->disable();
   }
   emit scriptLoadedChanged();
}

void AutoSignScriptProvider::init(const QString &filename)
{
   if (filename.isEmpty()) {
      return;
   }
   scriptLoaded_ = false;
   scriptRunner_->enable(filename);
   emit scriptLoadedChanged();
}

void AutoSignScriptProvider::deinit()
{
   scriptRunner_->disable();
   scriptLoaded_ = false;
   emit scriptLoadedChanged();
}

void AutoSignScriptProvider::onScriptLoaded(const QString &filename)
{
   logger_->info("[AutoSignScriptProvider::onScriptLoaded] script {} loaded"
      , filename.toStdString());
   scriptLoaded_ = true;
   emit scriptLoadedChanged();

   auto scripts = appSettings_->get<QStringList>(ApplicationSettings::aqScripts);
   if (scripts.indexOf(filename) < 0) {
      scripts << filename;
      appSettings_->set(ApplicationSettings::aqScripts, scripts);
   }
   appSettings_->set(lastScript_, filename);
   emit scriptHistoryChanged();
}

void AutoSignScriptProvider::onScriptFailed(const QString &filename, const QString &error)
{
   logger_->error("[AutoSignScriptProvider::onScriptLoaded] script {} loading failed: {}"
      , filename.toStdString(), error.toStdString());
   setScriptLoaded(false);

   auto scripts = appSettings_->get<QStringList>(ApplicationSettings::aqScripts);
   scripts.removeOne(filename);
   appSettings_->set(ApplicationSettings::aqScripts, scripts);
   appSettings_->reset(lastScript_);
   emit scriptHistoryChanged();
}

void AutoSignScriptProvider::onConnectedToCeler()
{
   emit autoSignQuoteAvailabilityChanged();
}

void AutoSignScriptProvider::onDisconnectedFromCeler()
{
   scriptRunner_->disable();
   disableAutoSign();

   emit autoSignQuoteAvailabilityChanged();
}

void AutoSignScriptProvider::setWalletsManager(std::shared_ptr<bs::sync::WalletsManager> &walletsMgr)
{
   walletsManager_ = walletsMgr;
   scriptRunner_->setWalletsManager(walletsMgr);

   connect(walletsMgr.get(), &bs::sync::WalletsManager::walletDeleted, this
      , &AutoSignScriptProvider::autoSignQuoteAvailabilityChanged);
   connect(walletsMgr.get(), &bs::sync::WalletsManager::walletAdded, this
      , &AutoSignScriptProvider::autoSignQuoteAvailabilityChanged);
   connect(walletsMgr.get(), &bs::sync::WalletsManager::walletsReady, this
      , &AutoSignScriptProvider::autoSignQuoteAvailabilityChanged);
   connect(walletsMgr.get(), &bs::sync::WalletsManager::walletsSynchronized, this
      , &AutoSignScriptProvider::autoSignQuoteAvailabilityChanged);
   connect(walletsMgr.get(), &bs::sync::WalletsManager::newWalletAdded, this
      , &AutoSignScriptProvider::autoSignQuoteAvailabilityChanged);
   connect(walletsMgr.get(), &bs::sync::WalletsManager::walletImportFinished, this
      , &AutoSignScriptProvider::autoSignQuoteAvailabilityChanged);

   emit autoSignQuoteAvailabilityChanged();
}

QString AutoSignScriptProvider::getAutoSignWalletName()
{
   if (!walletsManager_ || !signingContainer_) {
      return QString();
   }

   const auto wallet = walletsManager_->getPrimaryWallet();
   if (!wallet) {
      return QString();
   }
   return QString::fromStdString(wallet->name());
}

QString AutoSignScriptProvider::getDefaultScriptsDir()
{
#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
   return QCoreApplication::applicationDirPath() + QStringLiteral("/scripts");
#else
   return QStringLiteral("/usr/share/blocksettle/scripts");
#endif
}

QStringList AutoSignScriptProvider::getScripts()
{
   return appSettings_->get<QStringList>(ApplicationSettings::aqScripts);
}

QString AutoSignScriptProvider::getLastScript()
{
   return appSettings_->get<QString>(lastScript_);
}

QString AutoSignScriptProvider::getLastDir()
{
   return appSettings_->get<QString>(ApplicationSettings::LastAqDir);
}

void AutoSignScriptProvider::setLastDir(const QString &path)
{
   appSettings_->set(ApplicationSettings::LastAqDir, QFileInfo(path).dir().absolutePath());
}


AutoSignAQProvider::AutoSignAQProvider(const std::shared_ptr<spdlog::logger> &logger
   , UserScriptRunner *scriptRunner
   , const std::shared_ptr<ApplicationSettings> &appSettings
   , const std::shared_ptr<HeadlessContainer> &container
   , const std::shared_ptr<CelerClientQt> &celerClient
   , QObject *parent)
   : AutoSignScriptProvider(logger, scriptRunner, appSettings, container, celerClient, parent)
{
   auto botFileInfo = QFileInfo(getDefaultScriptsDir() + QStringLiteral("/RFQBot.qml"));
   if (botFileInfo.exists() && botFileInfo.isFile()) {
      auto list = appSettings_->get<QStringList>(ApplicationSettings::aqScripts);
      if (list.indexOf(botFileInfo.absoluteFilePath()) == -1) {
         list << botFileInfo.absoluteFilePath();
      }
      appSettings_->set(ApplicationSettings::aqScripts, list);
      const auto lastScript = appSettings_->get<QString>(ApplicationSettings::lastAqScript);
      if (lastScript.isEmpty()) {
         appSettings_->set(ApplicationSettings::lastAqScript, botFileInfo.absoluteFilePath());
      }
   }
   lastScript_ = ApplicationSettings::lastAqScript;
}


AutoSignRFQProvider::AutoSignRFQProvider(const std::shared_ptr<spdlog::logger> &logger
   , UserScriptRunner *scriptRunner
   , const std::shared_ptr<ApplicationSettings> &appSettings
   , const std::shared_ptr<HeadlessContainer> &container
   , const std::shared_ptr<CelerClientQt> &celerClient
   , QObject *parent)
   : AutoSignScriptProvider(logger, scriptRunner, appSettings, container, celerClient, parent)
{
   auto botFileInfo = QFileInfo(getDefaultScriptsDir() + QStringLiteral("/AutoRFQ.qml"));
   if (botFileInfo.exists() && botFileInfo.isFile()) {
      auto list = appSettings_->get<QStringList>(ApplicationSettings::aqScripts);
      if (list.indexOf(botFileInfo.absoluteFilePath()) == -1) {
         list << botFileInfo.absoluteFilePath();
      }
      appSettings_->set(ApplicationSettings::aqScripts, list);
      const auto lastScript = appSettings_->get<QString>(ApplicationSettings::CurrentRFQScript);
      if (lastScript.isEmpty()) {
         appSettings_->set(ApplicationSettings::CurrentRFQScript, botFileInfo.absoluteFilePath());
      }
   }
   lastScript_ = ApplicationSettings::CurrentRFQScript;
}
