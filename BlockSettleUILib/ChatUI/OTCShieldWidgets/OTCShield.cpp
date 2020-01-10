/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "OTCShield.h"
#include "AuthAddressManager.h"
#include "Wallets/SyncWalletsManager.h"

namespace {
   const QString shieldLoginToAccessOTC = QObject::tr("Login to access OTC chat");
   const QString shieldOtcAvailableToTradingParticipants = QObject::tr("OTC available to Trading Participants");
   const QString shieldCounterPartyIsntTradingParticipant = QObject::tr("Counterparty isn't a Trading Participant");
   const QString shieldContactIsOffline = QObject::tr("Contact is offline");
   const QString shieldOtcAvailableOnlyForAcceptedContacts = QObject::tr("OTC available only for Accepted contacts");
   const QString shieldOtcSetupTransactionData = QObject::tr("Setup OTC transaction data");

   const QString tradingKeyWord = QObject::tr("trading");

   const QString publicChatHeader = QObject::tr("Public chat");
   const QString privateChatHeader = QObject::tr("Private chat");
   const QString publicChatExplanation = QObject::tr("ChatID is a hash of the users email\n"
      "Public rooms are unencrypted\n"
      "(Trolls will be banned)\n");
   const QString privateChatExplanation = QObject::tr("Communication is end-to-end encrypted with the users key-pair(s)\n"
      "Trades can be negotiated in private chats\n"
      "Price and volume will not be disclosed\n");
}

OTCShield::OTCShield(QWidget* parent)
   : WalletShieldBase(parent)
{
   tabType_ = tradingKeyWord;
}

OTCShield::~OTCShield() noexcept = default;

void OTCShield::showLoginToAccessOTC()
{
   showShield(shieldLoginToAccessOTC);
}

void OTCShield::showOtcAvailableToTradingParticipants()
{
   showShield(shieldOtcAvailableToTradingParticipants);
}

void OTCShield::showCounterPartyIsntTradingParticipant()
{
   showShield(shieldCounterPartyIsntTradingParticipant);
}

void OTCShield::showContactIsOffline()
{
   showShield(shieldContactIsOffline);
}

void OTCShield::showOtcAvailableOnlyForAcceptedContacts()
{
   showShield(shieldOtcAvailableOnlyForAcceptedContacts);
}

void OTCShield::showOtcSetupTransaction()
{
   showShield(shieldOtcSetupTransactionData);
}

void OTCShield::showChatExplanation()
{
   showTwoBlockShield(publicChatHeader, publicChatExplanation,
      privateChatHeader, privateChatExplanation);
}

bool OTCShield::onRequestCheckWalletSettings()
{
   return checkWalletSettings(productType_, product_);
}
