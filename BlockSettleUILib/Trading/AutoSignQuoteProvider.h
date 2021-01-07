/*

***********************************************************************************
* Copyright (C) 2019 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef AUTOSIGNQUOTEPROVIDER_H
#define AUTOSIGNQUOTEPROVIDER_H

#include "BSErrorCode.h"

#include <QObject>
#include <memory>
#include "ApplicationSettings.h"

class CelerClientQt;
class HeadlessContainer;
class UserScriptRunner;
class AssetManager;
class QuoteProvider;
class MDCallbacksQt;

namespace bs {
   namespace sync {
      class Wallet;
      class WalletsManager;
   }
}
namespace spdlog {
   class logger;
}


class AutoSignScriptProvider : public QObject
{
   Q_OBJECT
public:
   explicit AutoSignScriptProvider(const std::shared_ptr<spdlog::logger> &
      , UserScriptRunner *
      , const std::shared_ptr<ApplicationSettings> &
      , const std::shared_ptr<HeadlessContainer> &
      , const std::shared_ptr<CelerClientQt> &
      , QObject *parent = nullptr);

   bool isScriptLoaded() const { return scriptLoaded_; }
   void setScriptLoaded(bool loaded);

   void init(const QString &filename);
   void deinit();

   static QString getDefaultScriptsDir();

   QStringList getScripts();
   QString getLastScript();

   QString getLastDir();
   void setLastDir(const QString &path);

   // auto sign
   bs::error::ErrorCode autoSignState() const { return autoSignState_; }
   QString autoSignWalletId() const { return autoSignWalletId_; }

   void disableAutoSign();
   void tryEnableAutoSign();

   bool isReady() const;
   void setWalletsManager(std::shared_ptr<bs::sync::WalletsManager> &);

   QString getAutoSignWalletName();

   UserScriptRunner *scriptRunner() const { return scriptRunner_; }

public slots:
   void onSignerStateUpdated();
   void onAutoSignStateChanged(bs::error::ErrorCode result, const std::string &walletId);

   void onScriptLoaded(const QString &filename);
   void onScriptFailed(const QString &filename, const QString &error);

   void onConnectedToCeler();
   void onDisconnectedFromCeler();

signals:
   void scriptHistoryChanged();
   void autoSignStateChanged();
   void autoSignQuoteAvailabilityChanged();
   void scriptLoadedChanged();

protected:
   std::shared_ptr<ApplicationSettings>       appSettings_;
   ApplicationSettings::Setting  lastScript_{ ApplicationSettings::_last };
   std::shared_ptr<spdlog::logger>           logger_;
   std::shared_ptr<HeadlessContainer>        signingContainer_;
   std::shared_ptr<bs::sync::WalletsManager> walletsManager_;
   std::shared_ptr<CelerClientQt>            celerClient_;
   UserScriptRunner *scriptRunner_{};

   bs::error::ErrorCode  autoSignState_{ bs::error::ErrorCode::AutoSignDisabled };
   QString  autoSignWalletId_;
   bool     scriptLoaded_{ false };
   bool     celerConnected_{ false };
   bool     newLoaded_{ false };
};

class AutoSignAQProvider : public AutoSignScriptProvider
{
   Q_OBJECT
public:
   explicit AutoSignAQProvider(const std::shared_ptr<spdlog::logger> &
      , UserScriptRunner *
      , const std::shared_ptr<ApplicationSettings> &
      , const std::shared_ptr<HeadlessContainer> &
      , const std::shared_ptr<CelerClientQt> &
      , QObject *parent = nullptr);
};

class AutoSignRFQProvider : public AutoSignScriptProvider
{
   Q_OBJECT
public:
   explicit AutoSignRFQProvider(const std::shared_ptr<spdlog::logger> &
      , UserScriptRunner *
      , const std::shared_ptr<ApplicationSettings> &
      , const std::shared_ptr<HeadlessContainer> &
      , const std::shared_ptr<CelerClientQt> &
      , QObject *parent = nullptr);
};

#endif // AUTOSIGNQUOTEPROVIDER_H
