#ifndef __CREATE_OTC_RESPONSE_WIDGET_H__
#define __CREATE_OTC_RESPONSE_WIDGET_H__

#include <QWidget>
#include <memory>

namespace Ui {
   class CreateOTCResponseWidget;
};

class CreateOTCResponseWidget : public QWidget
{
Q_OBJECT

public:
   CreateOTCResponseWidget(QWidget* parent = nullptr);
   ~CreateOTCResponseWidget() override;

private:
   std::unique_ptr<Ui::CreateOTCResponseWidget>    ui_;
};

#endif // __CREATE_OTC_RESPONSE_WIDGET_H__
