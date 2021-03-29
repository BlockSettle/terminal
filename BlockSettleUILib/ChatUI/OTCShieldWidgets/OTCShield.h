/*

***********************************************************************************
* Copyright (C) 2019 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __OTC_SHIELD_H__
#define __OTC_SHIELD_H__

#include "Trading/WalletShieldBase.h"

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
   void showShieldOtcAvailableOnceAccepted();

public slots:
   bool onRequestCheckWalletSettings();

private:

   const ProductType productType_ = ProductType::SpotXBT;
   const QString product_ = QLatin1String("XBT/EUR");
};

#endif // __OTC_SHIELD_H__
