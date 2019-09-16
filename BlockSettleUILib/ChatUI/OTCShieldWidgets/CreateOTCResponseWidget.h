#ifndef __CREATE_OTC_RESPONSE_WIDGET_H__
#define __CREATE_OTC_RESPONSE_WIDGET_H__

#include <QWidget>
#include <memory>
#include "OTCWindowsAdapterBase.h"

namespace Ui {
   class CreateOTCResponseWidget;
};

class CreateOTCResponseWidget : public OTCWindowsAdapterBase
{
   Q_OBJECT
public:
   CreateOTCResponseWidget(QWidget* parent = nullptr);
   ~CreateOTCResponseWidget() override;

private:
   std::unique_ptr<Ui::CreateOTCResponseWidget>    ui_;
};

#endif // __CREATE_OTC_RESPONSE_WIDGET_H__
