#ifndef __CREATE_OTC_REQUEST_WIDGET_H__
#define __CREATE_OTC_REQUEST_WIDGET_H__

#include <QWidget>
#include <memory>

namespace Ui {
   class CreateOTCRequestWidget;
};


class CreateOTCRequestWidget : public QWidget
{
public:
   CreateOTCRequestWidget(QWidget* parent = nullptr);
   ~CreateOTCRequestWidget() override;

private slots:
   void onSelectXBT();
   void onSelectEUR();
   void onSelectBuy();
   void onSelectSell();

private:
   std::unique_ptr<Ui::CreateOTCRequestWidget> ui_;
};

#endif // __CREATE_OTC_REQUEST_WIDGET_H__
