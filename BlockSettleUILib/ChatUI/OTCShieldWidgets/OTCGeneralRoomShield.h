#ifndef __OTC_GENERAL_ROOM_SHIELD_H__
#define __OTC_GENERAL_ROOM_SHIELD_H__

#include <QWidget>

#include <memory>

namespace Ui {
    class OTCGeneralRoomShield;
}

class OTCGeneralRoomShield : public QWidget
{
Q_OBJECT

public:
   explicit OTCGeneralRoomShield(QWidget* parent = nullptr );
   ~OTCGeneralRoomShield() noexcept override;

private:
   std::unique_ptr<Ui::OTCGeneralRoomShield> ui_;
};

#endif // __OTC_GENERAL_ROOM_SHIELD_H__
