#include "OTCShield.h"
#include "ui_OTCShield.h"
#include <QStackedWidget>

namespace {
   const QString shieldLoginToAccessOTC = QObject::tr("Login to access OTC chat");
   const QString shieldOtcUnavailableGlobal = QObject::tr("OTC unavailable in Global");
   const QString shieldOtcUnavailableSupport = QObject::tr("OTC unavailable in Support");
   const QString shieldOtcAvailableToTradingParticipants = QObject::tr("OTC available to Trading Participants");
   const QString shieldCounterPartyIsntTradingParticipant = QObject::tr("Counter party isn't a Trading Participant");
   const QString shieldContactIsOffline = QObject::tr("Contact is offline");
   const QString shieldOtcAvailableOnlyForAcceptedContacts = QObject::tr("OTC available only for Accepted contacts");
}

OTCShield::OTCShield(QWidget* parent)
   : QWidget(parent)
   , ui_(new Ui::OTCShield())
{
   ui_->setupUi(this);
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

void OTCShield::showShield(const QString& shieldText)
{
   ui_->shieldLabel->setText(shieldText);

   QStackedWidget* stack = qobject_cast<QStackedWidget*>(parent());

   // We expected that shield widget will leave only under stack widget
   Q_ASSERT(stack);
   if (!stack) {
      return;
   }

   stack->setCurrentWidget(this);
}
