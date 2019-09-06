#ifndef AUTOSIGNQUOTEPROVIDER_H
#define AUTOSIGNQUOTEPROVIDER_H

#include <QObject>
#include <memory>

class ApplicationSettings;
class BaseCelerClient;
class SignContainer;
class UserScriptRunner;
class AssetManager;
class QuoteProvider;
class MarketDataProvider;

namespace bs {
   class DealerUtxoResAdapter;
   namespace sync {
      class Wallet;
      class WalletsManager;
   }
}

namespace spdlog {
   class logger;
}

// Auto quoting and signing provider
class AutoSignQuoteProvider : public QObject
{
   Q_OBJECT
public:
   explicit AutoSignQuoteProvider(const std::shared_ptr<spdlog::logger> &
      , const std::shared_ptr<AssetManager>&
      , const std::shared_ptr<QuoteProvider>&
      , const std::shared_ptr<ApplicationSettings> &
      , const std::shared_ptr<bs::DealerUtxoResAdapter> &
      , const std::shared_ptr<SignContainer> &
      , const std::shared_ptr<MarketDataProvider> &
      , const std::shared_ptr<BaseCelerClient> &
      , QObject *parent = nullptr);

   // auto quote
   bool aqLoaded() const;
   void setAqLoaded(bool loaded);

   void initAQ(const QString &filename);
   void deinitAQ();

   QString getDefaultScriptsDir();

   QStringList getAQScripts();
   QString getAQLastScript();

   QString getAQLastDir();
   void setAQLastDir(const QString &path);

   // auto sign
   bool autoSignState() const;

   void disableAutoSign();
   void tryEnableAutoSign();

   //
   bool autoSignQuoteAvailable();
   void setWalletsManager(std::shared_ptr<bs::sync::WalletsManager> &);

   QString getAutoSignWalletName();

   UserScriptRunner *autoQuoter() const;

signals:
   void aqScriptLoaded(const QString &filename);
   void aqScriptUnLoaded();
   void aqHistoryChanged();
   void autoSignStateChanged(const std::string &walletId, bool active);
   void autoSignQuoteAvailabilityChanged();

public slots:
   void onSignerStateUpdated();
   void onAutoSignStateChanged(const std::string &walletId, bool active);

   void onAqScriptLoaded(const QString &filename);
   void onAqScriptFailed(const QString &filename, const QString &error);

   void onConnectedToCeler();
   void onDisconnectedFromCeler();

private:
   bool              autoSignState_{false};
   UserScriptRunner *aq_{};
   bool              aqLoaded_{false};
   bool              celerConnected_{false};
   bool              newLoaded_{false};

   std::shared_ptr<ApplicationSettings>       appSettings_;
   std::shared_ptr<spdlog::logger>            logger_;
   std::shared_ptr<SignContainer>             signingContainer_;
   std::shared_ptr<bs::sync::WalletsManager>  walletsManager_;
   std::shared_ptr<BaseCelerClient>           celerClient_;
};

#endif // AUTOSIGNQUOTEPROVIDER_H
