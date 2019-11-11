#ifndef __OTC_SHIELD_H__
#define __OTC_SHIELD_H__

#include "WalletShieldBase.h"

class OTCShield : public WalletShieldBase
{
Q_OBJECT

public:
   explicit OTCShield(QWidget* parent = nullptr );
   ~OTCShield() noexcept override;
  
   void showLoginToAccessOTC();
   void showOtcAvailableToTradingParticipants();
   void showCounterPartyIsntTradingParticipant();
   void showContactIsOffline();
   void showOtcAvailableOnlyForAcceptedContacts();
   void showOtcSetupTransaction();
   void showChatExplanation();

public slots:
   bool onRequestCheckWalletSettings();

private:

   const ProductType productType_ = ProductType::SpotXBT;
   const QString product_ = QLatin1String("XBT/EUR");
};

#endif // __OTC_SHIELD_H__
