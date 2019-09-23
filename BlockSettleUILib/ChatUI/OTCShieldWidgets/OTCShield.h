#ifndef __OTC_SHIELD_H__
#define __OTC_SHIELD_H__

#include <QWidget>
#include <memory>

namespace Ui {
    class OTCShield;
}

class OTCShield : public QWidget
{
Q_OBJECT

public:
   explicit OTCShield(QWidget* parent = nullptr );
   ~OTCShield() noexcept override;

   void showLoginToAccessOTC();
   void showOtcUnavailableGlobal();
   void showOtcUnavailableSupport();
   void showOtcAvailableToTradingParticipants();
   void showCounterPartyIsntTradingParticipant();
   void showContactIsOffline();
   void showOtcAvailableOnlyForAcceptedContacts();

protected:
   void showShield(const QString& shieldText);

private:
   std::unique_ptr<Ui::OTCShield> ui_;
};

#endif // __OTC_SHIELD_H__
