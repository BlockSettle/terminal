#ifndef __RFQ_REQUEST_WIDGET_H__
#define __RFQ_REQUEST_WIDGET_H__

#include <QWidget>
#include <QTimer>
#include <memory>
#include "CommonTypes.h"
#include "TabWithShortcut.h"

namespace Ui {
    class RFQRequestWidget;
}

class ApplicationSettings;
class AssetManager;
class AuthAddressManager;
class CelerClient;
class DialogManager;
class MarketDataProvider;
class QuoteProvider;
class SignContainer;
class WalletsManager;
class WalletsManager;


namespace spdlog
{
   class logger;
}

class RFQRequestWidget : public TabWithShortcut
{
Q_OBJECT

public:
   RFQRequestWidget(QWidget* parent = nullptr);
   ~RFQRequestWidget() = default;

   void init(std::shared_ptr<spdlog::logger> logger
         , const std::shared_ptr<CelerClient>& celerClient
         , const std::shared_ptr<AuthAddressManager> &
         , std::shared_ptr<QuoteProvider> quoteProvider
         , const std::shared_ptr<MarketDataProvider>& mdProvider
         , const std::shared_ptr<AssetManager>& assetManager
         , const std::shared_ptr<ApplicationSettings> &appSettings
         , const std::shared_ptr<DialogManager> &dialogManager
         , const std::shared_ptr<SignContainer> &);
   void SetWalletsManager(const std::shared_ptr<WalletsManager> &walletsManager);

   void shortcutActivated(ShortcutType s) override;

public slots:
   void onRFQSubmit(const bs::network::RFQ& rfq);

private:
   Ui::RFQRequestWidget* ui_;

   std::shared_ptr<spdlog::logger>     logger_;
   std::shared_ptr<CelerClient>        celerClient_;
   std::shared_ptr<QuoteProvider>      quoteProvider_;
   std::shared_ptr<AssetManager>       assetManager_;
   std::shared_ptr<AuthAddressManager> authAddressManager_;
   std::shared_ptr<DialogManager>      dialogManager_;

   std::shared_ptr<WalletsManager>     walletsManager_;
   std::shared_ptr<SignContainer>      signingContainer_;
};

#endif // __RFQ_REQUEST_WIDGET_H__
