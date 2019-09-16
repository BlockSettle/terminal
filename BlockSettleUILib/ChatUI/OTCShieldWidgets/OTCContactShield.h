#ifndef OTCCONTACTSHIELD_H
#define OTCCONTACTSHIELD_H

#include <QWidget>

namespace Ui {
   class OTCContactShield;
   }

class OTCContactShield : public QWidget
{
   Q_OBJECT

public:
   explicit OTCContactShield(QWidget *parent = nullptr);
   ~OTCContactShield();

private:
   Ui::OTCContactShield *ui;
};

#endif // OTCCONTACTSHIELD_H
