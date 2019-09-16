#ifndef OTCSUPPORTROOMSHIELD_H
#define OTCSUPPORTROOMSHIELD_H

#include <QWidget>

#include <memory>

namespace Ui {
   class OTCSupportRoomShield;
}

class OTCSupportRoomShield : public QWidget
{
   Q_OBJECT

public:
   explicit OTCSupportRoomShield(QWidget *parent = nullptr);
   ~OTCSupportRoomShield() noexcept override;

private:
   std::unique_ptr<Ui::OTCSupportRoomShield> ui_;
};

#endif // OTCSUPPORTROOMSHIELD_H
