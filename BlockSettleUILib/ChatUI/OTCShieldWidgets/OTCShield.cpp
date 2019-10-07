#include "OTCShield.h"
#include "AuthAddressManager.h"
#include "Wallets/SyncWalletsManager.h"

namespace {
   const QString shieldLoginToAccessOTC = QObject::tr("Login to access OTC chat");
   const QString shieldOtcUnavailableGlobal = QObject::tr("OTC unavailable in Global");
   const QString shieldOtcUnavailableSupport = QObject::tr("OTC unavailable in Support");
   const QString shieldOtcAvailableToTradingParticipants = QObject::tr("OTC available to Trading Participants");
   const QString shieldCounterPartyIsntTradingParticipant = QObject::tr("Counter party isn't a Trading Participant");
   const QString shieldContactIsOffline = QObject::tr("Contact is offline");
   const QString shieldOtcAvailableOnlyForAcceptedContacts = QObject::tr("OTC available only for Accepted contacts");
   const QString shieldOtcSetupTransactionData = QObject::tr("Setup OTC transaction data");

   const QString tradingKeyWord = QObject::tr("trading");
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

void OTCShield::showOtcUnavailableGlobal()
{
   showShield(shieldOtcUnavailableGlobal);
}

void OTCShield::showOtcUnavailableSupport()
{
   showShield(shieldOtcUnavailableSupport);
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

bool OTCShield::onRequestCheckWalletSettings()
{
   return checkWalletSettings(productType_, product_);
}
