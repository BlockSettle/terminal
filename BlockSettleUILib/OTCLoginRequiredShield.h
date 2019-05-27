#ifndef __OTC_LOGIN_REQUIRED_SHIELD_H__
#define __OTC_LOGIN_REQUIRED_SHIELD_H__

#include <QWidget>

#include <memory>

namespace Ui {
    class OTCLoginRequiredShield;
}

class OTCLoginRequiredShield : public QWidget
{
Q_OBJECT

public:
   explicit OTCLoginRequiredShield(QWidget* parent = nullptr );
   ~OTCLoginRequiredShield() noexcept override;

private:
   std::unique_ptr<Ui::OTCLoginRequiredShield> ui_;
};

#endif // __OTC_LOGIN_REQUIRED_SHIELD_H__
