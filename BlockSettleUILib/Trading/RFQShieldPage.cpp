/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "RFQShieldPage.h"

namespace {
   // Label texts
   const QString shieldLoginToSubmitRFQs = QObject::tr("Login to submit RFQs");
   const QString shieldLoginToResponseRFQs = QObject::tr("Login to submit responsive quotes");
   const QString shieldTradingParticipantOnly = QObject::tr("Reserved for Trading Participants");
   const QString shieldDealingParticipantOnly = QObject::tr("Reserved for Dealing Participants");
   const QString shieldTradeUnselectedTargetRequest = QObject::tr("In the Market Data window, please click on the product / security you wish to trade");
   const QString shieldDealingUnselectedTargetRequest = QObject::tr("In the Quote Request Blotter, please click on the product / security you wish to quote");
}

RFQShieldPage::RFQShieldPage(QWidget *parent) :
   WalletShieldBase(parent)
{
}

RFQShieldPage::~RFQShieldPage() noexcept = default;

void RFQShieldPage::showShieldLoginToSubmitRequired()
{
   showShield(shieldLoginToSubmitRFQs);
}

void RFQShieldPage::showShieldLoginToResponseRequired()
{
   showShield(shieldLoginToResponseRFQs);
}

void RFQShieldPage::showShieldReservedTradingParticipant()
{
   showShield(shieldTradingParticipantOnly);
}

void RFQShieldPage::showShieldReservedDealingParticipant()
{
   showShield(shieldDealingParticipantOnly);
}

void RFQShieldPage::showShieldSelectTargetTrade()
{
   showShield(shieldTradeUnselectedTargetRequest);
}

void RFQShieldPage::showShieldSelectTargetDealing()
{
   showShield(shieldDealingUnselectedTargetRequest);
}
