#include "AutoSignQuoteProvider.h"

#include "ApplicationSettings.h"
#include "SignContainer.h"
#include "WalletManager.h"
#include "Wallets/SyncWalletsManager.h"
#include "Wallets/SyncHDWallet.h"
#include "UserScriptRunner.h"

#include <BaseCelerClient.h>
#include <QCoreApplication>
#include <QFileDialog>
#include <QFileInfo>

AutoSignQuoteProvider::AutoSignQuoteProvider(const std::shared_ptr<spdlog::logger> &logger
   , const std::shared_ptr<AssetManager>& assetManager
   , const std::shared_ptr<QuoteProvider>& quoteProvider
   , const std::shared_ptr<ApplicationSettings> &appSettings
   , const std::shared_ptr<SignContainer> &container
   , const std::shared_ptr<MarketDataProvider> &mdProvider
   , const std::shared_ptr<BaseCelerClient> &celerClient
   , QObject *parent)
   : QObject(parent)
   , appSettings_(appSettings)
   , logger_(logger)
   , signingContainer_(container)
   , celerClient_(celerClient)
{
   aq_ = new UserScriptRunner(quoteProvider, container,
      mdProvider, assetManager, logger, this);

   if (walletsManager_) {
      aq_->setWalletsManager(walletsManager_);
   }

   if (signingContainer_) {
      connect(signingContainer_.get(), &SignContainer::ready, this, &AutoSignQuoteProvider::onSignerStateUpdated, Qt::QueuedConnection);
      connect(signingContainer_.get(), &SignContainer::disconnected, this, &AutoSignQuoteProvider::onSignerStateUpdated, Qt::QueuedConnection);
      connect(signingContainer_.get(), &SignContainer::AutoSignStateChanged, this, &AutoSignQuoteProvider::onAutoSignStateChanged);
   }

   connect(aq_, &UserScriptRunner::aqScriptLoaded, this, &AutoSignQuoteProvider::onAqScriptLoaded);
   connect(aq_, &UserScriptRunner::failedToLoad, this, &AutoSignQuoteProvider::onAqScriptFailed);

   onSignerStateUpdated();

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

   connect(celerClient_.get(), &BaseCelerClient::OnConnectedToServer, this, &AutoSignQuoteProvider::onConnectedToCeler);
   connect(celerClient_.get(), &BaseCelerClient::OnConnectionClosed, this, &AutoSignQuoteProvider::onDisconnectedFromCeler);
}

void AutoSignQuoteProvider::onSignerStateUpdated()
{
   disableAutoSign();
   autoQuoter()->disableAQ();

   emit autoSignQuoteAvailabilityChanged();
}

void AutoSignQuoteProvider::disableAutoSign()
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

void AutoSignQuoteProvider::tryEnableAutoSign()
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

bool AutoSignQuoteProvider::autoSignQuoteAvailable()
{
   return signingContainer_ && !signingContainer_->isOffline()
      && walletsManager_ && walletsManager_->isReadyForTrading()
      && celerClient_->IsConnected();
}

bool AutoSignQuoteProvider::aqLoaded() const
{
   return aqLoaded_;
}

void AutoSignQuoteProvider::onAutoSignStateChanged(bs::error::ErrorCode result, const std::string &walletId)
{
   autoSignState_ = result;
   autoSignWalletId_ = QString::fromStdString(walletId);
   emit autoSignStateChanged();
}

void AutoSignQuoteProvider::setAqLoaded(bool loaded)
{
   aqLoaded_ = loaded;
   if (!loaded) {
      aq_->disableAQ();
   }
}

void AutoSignQuoteProvider::initAQ(const QString &filename)
{
   if (filename.isEmpty()) {
      return;
   }
   aqLoaded_ = false;
   aq_->enableAQ(filename);
}

void AutoSignQuoteProvider::deinitAQ()
{
   aq_->disableAQ();
   aqLoaded_ = false;
   emit aqScriptUnLoaded();
}

void AutoSignQuoteProvider::onAqScriptLoaded(const QString &filename)
{
   logger_->info("AQ script loaded ({})", filename.toStdString());
   aqLoaded_ = true;

   auto scripts = appSettings_->get<QStringList>(ApplicationSettings::aqScripts);
   if (scripts.indexOf(filename) < 0) {
      scripts << filename;
      appSettings_->set(ApplicationSettings::aqScripts, scripts);
   }
   appSettings_->set(ApplicationSettings::lastAqScript, filename);
   emit aqScriptLoaded(filename);
   emit aqHistoryChanged();
}

void AutoSignQuoteProvider::onAqScriptFailed(const QString &filename, const QString &error)
{
   logger_->error("AQ script loading failed (): {}", filename.toStdString(), error.toStdString());
   setAqLoaded(false);

   auto scripts = appSettings_->get<QStringList>(ApplicationSettings::aqScripts);
   scripts.removeOne(filename);
   appSettings_->set(ApplicationSettings::aqScripts, scripts);
   appSettings_->reset(ApplicationSettings::lastAqScript);
   emit aqHistoryChanged();
}

void AutoSignQuoteProvider::onConnectedToCeler()
{
   emit autoSignQuoteAvailabilityChanged();
}

void AutoSignQuoteProvider::onDisconnectedFromCeler()
{
   autoQuoter()->disableAQ();
   disableAutoSign();

   emit autoSignQuoteAvailabilityChanged();
}

UserScriptRunner *AutoSignQuoteProvider::autoQuoter() const
{
    return aq_;
}

bs::error::ErrorCode AutoSignQuoteProvider::autoSignState() const
{
    return autoSignState_;
}

void AutoSignQuoteProvider::setWalletsManager(std::shared_ptr<bs::sync::WalletsManager> &walletsMgr)
{
   walletsManager_ = walletsMgr;
   aq_->setWalletsManager(walletsMgr);

   connect(walletsMgr.get(), &bs::sync::WalletsManager::walletDeleted, this, &AutoSignQuoteProvider::autoSignQuoteAvailabilityChanged);
   connect(walletsMgr.get(), &bs::sync::WalletsManager::walletAdded, this, &AutoSignQuoteProvider::autoSignQuoteAvailabilityChanged);
   connect(walletsMgr.get(), &bs::sync::WalletsManager::walletsReady, this, &AutoSignQuoteProvider::autoSignQuoteAvailabilityChanged);
   connect(walletsMgr.get(), &bs::sync::WalletsManager::walletsSynchronized, this, &AutoSignQuoteProvider::autoSignQuoteAvailabilityChanged);
   connect(walletsMgr.get(), &bs::sync::WalletsManager::newWalletAdded, this, &AutoSignQuoteProvider::autoSignQuoteAvailabilityChanged);
   connect(walletsMgr.get(), &bs::sync::WalletsManager::walletImportFinished, this, &AutoSignQuoteProvider::autoSignQuoteAvailabilityChanged);

   emit autoSignQuoteAvailabilityChanged();
}

QString AutoSignQuoteProvider::getAutoSignWalletName()
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

QString AutoSignQuoteProvider::getDefaultScriptsDir()
{
#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
   return QCoreApplication::applicationDirPath() + QStringLiteral("/scripts");
#else
   return QStringLiteral("/usr/share/blocksettle/scripts");
#endif
}

QStringList AutoSignQuoteProvider::getAQScripts()
{
   return appSettings_->get<QStringList>(ApplicationSettings::aqScripts);
}

QString AutoSignQuoteProvider::getAQLastScript()
{
   return appSettings_->get<QString>(ApplicationSettings::lastAqScript);
}

QString AutoSignQuoteProvider::getAQLastDir()
{
   return appSettings_->get<QString>(ApplicationSettings::LastAqDir);
}

void AutoSignQuoteProvider::setAQLastDir(const QString &path)
{
   appSettings_->set(ApplicationSettings::LastAqDir, QFileInfo(path).dir().absolutePath());
}
