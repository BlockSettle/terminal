#include "CreateOTCRequestWidget.h"

#include "ui_CreateOTCRequestWidget.h"

CreateOTCRequestWidget::CreateOTCRequestWidget(QWidget* parent)
   : QWidget{parent}
   , ui_{new Ui::CreateOTCRequestWidget{}}
{
   ui_->setupUi(this);
   connect(ui_->btnXBT, &QPushButton::clicked,
      this, &CreateOTCRequestWidget::onSelectXBT);
   connect(ui_->btnEUR, &QPushButton::clicked,
      this, &CreateOTCRequestWidget::onSelectEUR);
   connect(ui_->btnBuy, &QPushButton::clicked,
      this, &CreateOTCRequestWidget::onSelectBuy);
   connect(ui_->btnSell, &QPushButton::clicked,
      this, &CreateOTCRequestWidget::onSelectSell);
}

CreateOTCRequestWidget::~CreateOTCRequestWidget() = default;

void CreateOTCRequestWidget::onSelectXBT()
{
   ui_->btnXBT->setChecked(true);
   ui_->btnEUR->setChecked(false);
}

void CreateOTCRequestWidget::onSelectEUR()
{
   ui_->btnXBT->setChecked(false);
   ui_->btnEUR->setChecked(true);
}

void CreateOTCRequestWidget::onSelectBuy()
{
   ui_->btnBuy->setChecked(true);
   ui_->btnSell->setChecked(false);
}

void CreateOTCRequestWidget::onSelectSell()
{
   ui_->btnBuy->setChecked(false);
   ui_->btnSell->setChecked(true);
}
