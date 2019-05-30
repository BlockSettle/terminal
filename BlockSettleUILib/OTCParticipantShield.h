#ifndef __OTC_PARTICIPANT_SHIELD_H__
#define __OTC_PARTICIPANT_SHIELD_H__

#include <QWidget>

#include <memory>

namespace Ui {
    class OTCParticipantShield;
}

class OTCParticipantShield : public QWidget
{
Q_OBJECT

public:
   explicit OTCParticipantShield(QWidget* parent = nullptr );
   ~OTCParticipantShield() noexcept override;

private:
   std::unique_ptr<Ui::OTCParticipantShield> ui_;
};

#endif // __OTC_PARTICIPANT_SHIELD_H__
