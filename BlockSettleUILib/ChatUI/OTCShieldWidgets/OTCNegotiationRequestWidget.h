#ifndef __OTC_NEGOTIATION_REQUEST_WIDGET_H__
#define __OTC_NEGOTIATION_REQUEST_WIDGET_H__

#include <QWidget>
#include <memory>
#include <set>

#include "OtcTypes.h"
#include "OTCWindowsAdapterBase.h"

namespace Ui {
   class OTCNegotiationCommonWidget;
};

namespace bs {
   namespace sync {
      namespace hd {
         class Wallet;
      }
   }
}

class OTCNegotiationRequestWidget : public OTCWindowsAdapterBase
{
Q_OBJECT
Q_DISABLE_COPY(OTCNegotiationRequestWidget)

public:
   OTCNegotiationRequestWidget(QWidget* parent = nullptr);
   ~OTCNegotiationRequestWidget() override;

   bs::network::otc::Offer offer();

signals:
   void requestCreated();

protected slots:
   void onSyncInterface() override;
   void onUpdateMD(bs::network::Asset::Type type, const QString &security, const bs::network::MDFields& fields) override;
   void onUpdateBalances() override;

protected:
   std::shared_ptr<bs::sync::hd::Wallet> getCurrentHDWallet() const;
   BTCNumericTypes::balance_type getXBTSpendableBalance() const;

private slots:
   void onSellClicked();
   void onBuyClicked();
   void onShowXBTInputsClicked();
   void onXbtInputsProcessed();
   void onChanged();
   void onChatRoomChanged();
   void onNumCcySelected();
   void onUpdateIndicativePrice();
   void onMaxQuantityClicked();

   void onCurrentWalletChanged();

private:
   std::unique_ptr<Ui::OTCNegotiationCommonWidget> ui_;

   bs::network::Asset::Type productGroup_ = bs::network::Asset::SpotXBT;
   QString security_{ QLatin1String("XBT/EUR") };
   QString sellProduct_{ QLatin1String("XBT") };
   QString buyProduct_{ QLatin1String("EUR") };
   double sellIndicativePrice_{};
   double buyIndicativePrice_{};
};

#endif // __OTC_NEGOTIATION_REQUEST_WIDGET_H__
