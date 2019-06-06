#ifndef OTCCONTACTNETSTATUSSHIELD_H
#define OTCCONTACTNETSTATUSSHIELD_H

#include <QWidget>

namespace Ui {
   class OTCContactNetStatusShield;
   }

class OTCContactNetStatusShield : public QWidget
{
   Q_OBJECT

public:
   explicit OTCContactNetStatusShield(QWidget *parent = nullptr);
   ~OTCContactNetStatusShield();

private:
   Ui::OTCContactNetStatusShield *ui;
};

#endif // OTCCONTACTNETSTATUSSHIELD_H
