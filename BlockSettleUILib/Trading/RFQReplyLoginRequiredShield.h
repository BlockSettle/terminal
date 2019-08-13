#ifndef RFQREPLYLOGINREQUIREDSHIELD_H
#define RFQREPLYLOGINREQUIREDSHIELD_H

#include <QWidget>

#include <memory>

namespace Ui {
   class RFQReplyLoginRequiredShield;
}

class RFQReplyLoginRequiredShield : public QWidget
{
   Q_OBJECT

public:
   explicit RFQReplyLoginRequiredShield(QWidget *parent = nullptr);
   ~RFQReplyLoginRequiredShield() noexcept;

private:
   std::unique_ptr<Ui::RFQReplyLoginRequiredShield> ui_;
};

#endif // RFQREPLYLOGINREQUIREDSHIELD_H
