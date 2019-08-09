#ifndef __RFQ_REQUEST_WIDGET_H__
#define __RFQ_REQUEST_WIDGET_H__

#include <QWidget>
#include <QTimer>
#include <memory>
#include "CommonTypes.h"
#include "TabWithShortcut.h"
#include "MarketDataWidget.h"

namespace Ui {
    class RFQRequestWidget;
}
namespace spdlog {
   class logger;
}
namespace bs {
   namespace sync {
      class WalletsManager;
   }
}
class ApplicationSettings;
class ArmoryConnection;
class AssetManager;
class AuthAddressManager;
class BaseCelerClient;
class ConnectionManager;
class DialogManager;
class MarketDataProvider;
class QuoteProvider;
class SignContainer;


class RFQRequestWidget : public TabWithShortcut
{
Q_OBJECT

public:
   RFQRequestWidget(QWidget* parent = nullptr);
   ~RFQRequestWidget() override;

   void initWidgets(const std::shared_ptr<MarketDataProvider>& mdProvider
      , const std::shared_ptr<ApplicationSettings> &appSettings);

   void init(std::shared_ptr<spdlog::logger> logger
         , const std::shared_ptr<BaseCelerClient>& celerClient
         , const std::shared_ptr<AuthAddressManager> &
         , std::shared_ptr<QuoteProvider> quoteProvider
         , const std::shared_ptr<AssetManager>& assetManager
         , const std::shared_ptr<DialogManager> &dialogManager
         , const std::shared_ptr<SignContainer> &
         , const std::shared_ptr<ArmoryConnection> &
         , const std::shared_ptr<ConnectionManager> &connectionManager
   );

   void setWalletsManager(const std::shared_ptr<bs::sync::WalletsManager> &);

   void shortcutActivated(ShortcutType s) override;

   void setAuthorized(bool authorized);

   

signals:
   void requestPrimaryWalletCreation();

private:
   void showShieldLoginRequiered();
   void showShieldReservedTraidingParticipant();
   void showShieldPromoteToPrimaryWallet();
   void showShieldCreateWallet();
   void showShieldCreateXXXLeaf(const QString& product);

   void showEditableRFQPage();
   void prepareAndPopShield(const QString& labelText, bool showButton = false, const QString& ButtonText = QLatin1String());

   bool checkConditions(const MarkeSelectedInfo& productGroup);
   bool checkWalletSettings(const MarkeSelectedInfo& productGroup);

public slots:
   void onRFQSubmit(const bs::network::RFQ& rfq);
   void onCurrencySelected(const MarkeSelectedInfo& selectedInfo);
   void onBuyClicked(const MarkeSelectedInfo& selectedInfo);
   void onSellClicked(const MarkeSelectedInfo& selectedInfo);
   void onDisableSelectedInfo();

private slots:
   void onConnectedToCeler();
   void onDisconnectedFromCeler();

public slots:
   void forceCheckCondition();

private:
   std::unique_ptr<Ui::RFQRequestWidget> ui_;

   std::shared_ptr<spdlog::logger>     logger_;
   std::shared_ptr<BaseCelerClient>        celerClient_;
   std::shared_ptr<QuoteProvider>      quoteProvider_;
   std::shared_ptr<AssetManager>       assetManager_;
   std::shared_ptr<AuthAddressManager> authAddressManager_;
   std::shared_ptr<DialogManager>      dialogManager_;

   std::shared_ptr<bs::sync::WalletsManager> walletsManager_;
   std::shared_ptr<SignContainer>      signingContainer_;
   std::shared_ptr<ArmoryConnection>   armory_;
   std::shared_ptr<ApplicationSettings> appSettings_;
   std::shared_ptr<ConnectionManager>  connectionManager_;

   QList<QMetaObject::Connection>   marketDataConnection;
};

#endif // __RFQ_REQUEST_WIDGET_H__
