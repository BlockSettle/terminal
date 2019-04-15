#ifndef __CREATE_OTC_RESPONSE_WIDGET_H__
#define __CREATE_OTC_RESPONSE_WIDGET_H__

#include <QWidget>
#include <memory>

#include "CommonTypes.h"

namespace Ui {
   class CreateOTCResponseWidget;
};


class CreateOTCResponseWidget : public QWidget
{
Q_OBJECT

public:
   CreateOTCResponseWidget(QWidget* parent = nullptr);
   ~CreateOTCResponseWidget() override;

   void SetSide(const bs::network::Side::Type& side);
   void SetRange(const bs::network::OTCRangeID& range);

public:
   bs::network::OTCPriceRange GetResponsePriceRange() const;
   bs::network::OTCQuantityRange GetResponseQuantityRange() const;

signals:
   void ResponseCreated();

private:
   std::unique_ptr<Ui::CreateOTCResponseWidget> ui_;
};

#endif // __CREATE_OTC_RESPONSE_WIDGET_H__
