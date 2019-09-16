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

protected:
   void syncInterface() override;
   std::shared_ptr<bs::sync::hd::Wallet> getCurrentHDWallet() const;
   

private slots:
   void onSellClicked();
   void onBuyClicked();
   void onShowXBTInputsClicked();
   void onShowXBTInputReady();
   void onChanged();
   void onChatRoomChanged();

   void onCurrentWalletChanged();

private:
   std::unique_ptr<Ui::OTCNegotiationCommonWidget> ui_;

   std::set<std::string> awaitingLeafsResponse;
   std::vector<UTXO> allUTXOs;
   std::vector<UTXO> selectedUTXO;
};

#endif // __OTC_NEGOTIATION_REQUEST_WIDGET_H__
