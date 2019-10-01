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

   void setPeer(const bs::network::otc::Peer &peer) override;

signals:
   void requestCreated();

public slots:
   void onAboutToApply() override;

protected slots:
   void onSyncInterface() override;
   void onMDUpdated() override;
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
   void toggleSideButtons(bool isSell);

   std::unique_ptr<Ui::OTCNegotiationCommonWidget> ui_;
};

#endif // __OTC_NEGOTIATION_REQUEST_WIDGET_H__
