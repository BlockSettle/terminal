#ifndef RFQREPLYLOGINREQUIREDSHIELD_H
#define RFQREPLYLOGINREQUIREDSHIELD_H

#include "WalletShieldBase.h"

class RFQShieldPage : public WalletShieldBase
{
   Q_OBJECT

public:
   explicit RFQShieldPage(QWidget *parent = nullptr);
   ~RFQShieldPage() noexcept override;

   void showShieldLoginToSubmitRequired();
   void showShieldLoginToResponseRequired();
   void showShieldReservedTradingParticipant();
   void showShieldReservedDealingParticipant();
   void showShieldSelectTargetTrade();
   void showShieldSelectTargetDealing();
};

#endif // RFQREPLYLOGINREQUIREDSHIELD_H
